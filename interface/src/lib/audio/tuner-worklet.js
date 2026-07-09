// ============================================================
// Hekatron SmartSonic Decoder - AudioWorklet processor
//
// Runs the DSP pipeline (NCO, IIR filters, decimation, phase
// demodulation, correlation, framing, Hamming decode, CRC16) off
// the main thread, on the dedicated audio rendering thread.
//
// This file is loaded via `audioContext.audioWorklet.addModule(url)`
// so it must be self-contained - no `import` statements, no DOM
// access. Communication with the main thread happens through
// `this.port.postMessage(...)`.
//
// Message protocol (worklet → main):
//   { type: 'log',   message: string }
//   { type: 'sync',  info: { quality: number, pattern: number } }
//   { type: 'frame', hexBytes: string[] }   // raw frame; main parses
//   { type: 'error', message: string }
//   { type: 'level', value: number }
// ============================================================

/* eslint-disable no-undef */

// --- Configuration ---
const SAMPLE_RATE = 44100;
const NCO_FREQ = 4449.1;
const DECIMATION = 8;
const SYNC_THRESHOLD = 300000;
const SYMBOL_CORRELATOR_EXTRA = 12;
const CLAMP = 400;

// --- IIR Filter coefficients (baked for SAMPLE_RATE = 44100 Hz) ---
const IIR_A1 = [1.0, -1.85716065, 0.86670342];
const IIR_B1 = [0.00238569, 0.00477138, 0.00238569];
const IIR_A2 = [1.0, -1.99194039, 0.99197274];
const IIR_B2 = [0.99597828, -1.99195657, 0.99597828];
const IIR_A_SMOOTH = [1.0, -3.22692923, 3.9658094, -2.19293563, 0.45943954];
const IIR_B_SMOOTH = [3.3651e-4, 0.00134602, 0.00201903, 0.00134602, 3.3651e-4];
const MEDIAN_SIZE = 5;

// --- Hamming codebook ---
const CODEBOOK = [0, 135, 153, 30, 170, 45, 51, 180, 75, 204, 210, 85, 225, 102, 120, 255];

// --- Bit correlator pattern (64 samples) ---
function buildBitPattern() {
	const p = new Int16Array(64);
	let idx = 0;
	for (let i = 0; i < 3; i++) p[idx++] = 0;
	for (let i = 0; i < 26; i++) p[idx++] = -1;
	for (let i = 0; i < 6; i++) p[idx++] = 0;
	for (let i = 0; i < 26; i++) p[idx++] = 1;
	for (let i = 0; i < 3; i++) p[idx++] = 0;
	return p;
}
const BIT_PATTERN = buildBitPattern();

function buildPattern(segs, totalLen) {
	const p = new Int16Array(totalLen);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}

const SYNC1 = buildPattern(
	[
		[65, 1],
		[132, -1],
		[33, 1],
		[33, -1],
		[163, 1],
		[132, -1],
		[98, 1],
		[66, -1],
		[33, 1],
		[33, -1],
		[32, 1],
		[33, -1],
		[66, 1],
		[99, -1],
		[130, 1],
		[166, -1],
		[32, 1],
		[33, -1],
		[131, 1],
		[66, -1]
	],
	1576
);

const SYNC2 = buildPattern(
	[
		[67, -1],
		[128, 1],
		[34, -1],
		[32, 1],
		[168, -1],
		[129, 1],
		[101, -1],
		[64, 1],
		[34, -1],
		[32, 1],
		[34, -1],
		[32, 1],
		[67, -1],
		[96, 1],
		[135, -1],
		[161, 1],
		[33, -1],
		[33, 1],
		[134, -1],
		[64, 1]
	],
	1578
);

const PATTERN_SET_1 = {
	id: 1,
	sync: SYNC1,
	bOffset: 133,
	cOffset: 1250,
	dOffset: 690,
	symbolLen: 65.708
};

const PATTERN_SET_2 = {
	id: 2,
	sync: SYNC2,
	bOffset: 130,
	cOffset: 1250,
	dOffset: 690,
	symbolLen: 65.802
};

// ========================
// DSP building blocks
// ========================

class IIRFilter {
	constructor(a, b) {
		this.a = a;
		this.b = b;
		this.state = new Float64Array(a.length).fill(0);
		this.order = a.length - 1;
	}
	process(x) {
		let y = this.b[0] * x + this.state[1];
		for (let i = 1; i < this.order; i++) {
			this.state[i] = this.state[i + 1] + this.b[i] * x - this.a[i] * y;
		}
		this.state[this.order] = this.b[this.order] * x - this.a[this.order] * y;
		return y;
	}
}

class MedianFilter {
	constructor(size) {
		this.size = size;
		this.buf = new Float64Array(size).fill(0);
		this.sorted = new Float64Array(size).fill(0);
		this.odd = size % 2 !== 0;
	}
	process(x) {
		for (let i = this.size - 1; i > 0; i--) {
			this.buf[i] = this.buf[i - 1];
			this.sorted[i] = this.buf[i - 1];
		}
		this.buf[0] = x;
		this.sorted[0] = x;
		this.sorted.sort();
		if (this.odd) return this.sorted[Math.floor(this.size / 2)];
		// Even size: average the two middle elements.
		const h = this.size / 2;
		return (this.sorted[h - 1] + this.sorted[h]) / 2;
	}
}

class Decimator {
	constructor(factor) {
		this.factor = factor;
		this.counter = 0;
		this.output = new Float64Array(0);
	}
	processInto(arr, len) {
		const maxOut = Math.ceil(len / this.factor) + 1;
		if (this.output.length < maxOut) this.output = new Float64Array(maxOut);
		let outIdx = 0;
		for (let i = 0; i < len; i++) {
			this.counter++;
			if (this.counter >= this.factor) {
				this.counter = 0;
				this.output[outIdx++] = arr[i];
			}
		}
		return outIdx;
	}
}

function correlate(signal, pattern, start, windowLen) {
	const patLen = pattern.length;
	const end = start + windowLen - patLen;
	let maxVal = 0,
		maxIdx = 0,
		minVal = 0,
		minIdx = 0;
	for (let i = start; i < end; i++) {
		let sum = 0;
		for (let j = 0; j < patLen; j++) sum += signal[i + j] * pattern[j];
		if (sum > maxVal) {
			maxVal = sum;
			maxIdx = i;
		}
		if (sum < minVal) {
			minVal = sum;
			minIdx = i;
		}
	}
	return { maxVal, maxIdx, minVal, minIdx };
}

function hammingDist(a, b) {
	let d = a ^ b,
		c = 0;
	for (let i = 0; i < 8; i++) if ((d >> i) & 1) c++;
	return c;
}

function hammingDecode(raw) {
	let bestDist = 8,
		bestIdx = 0;
	for (let i = 0; i < 16; i++) {
		const d = hammingDist(raw, CODEBOOK[i]);
		if (d < bestDist) {
			bestDist = d;
			bestIdx = i;
		}
	}
	return { value: bestIdx, errors: bestDist };
}

// ========================
// CRC16 (POLY=0x8005, init=0xFFFF, big-endian bit processing)
// ========================
function crc16(data, len) {
	const POLY = 0x8005;
	let crc = 0xffff;
	for (let i = 0; i < len; i++) {
		let b = data[i] & 0xff;
		for (let j = 0; j < 8; j++) {
			const xorBit = (((crc & 0x8000) >> 8) ^ (b & 0x80)) !== 0;
			crc = xorBit ? ((crc << 1) ^ POLY) & 0xffff : (crc << 1) & 0xffff;
			b = (b << 1) & 0xff;
		}
	}
	return crc;
}

// ========================
// TunerPipeline - port of the main-thread class.
//
// Identical to interface/src/lib/audio/tuner-pipeline.ts but adapted to:
// - run inside AudioWorkletGlobalScope (no DOM, no `Date.now()` budget)
// - post results via a callback wrapper instead of direct main-thread calls
// ========================

const RING_CAPACITY = 16384;

class TunerPipeline {
	constructor(callbacks, sampleRate) {
		this.onLog = callbacks.onLog;
		this.onSync = callbacks.onSync;
		this.onFrame = callbacks.onFrame;
		this.onError = callbacks.onError;

		const rate = sampleRate || SAMPLE_RATE;
		if (rate !== SAMPLE_RATE) {
			throw new Error(
				`TunerPipeline requires sampleRate=${SAMPLE_RATE} Hz (got ${rate} Hz); ` +
					`filter coefficients are baked for ${SAMPLE_RATE} Hz.`
			);
		}

		// Recursive NCO state.
		this.ncoCos = 1;
		this.ncoSin = 0;
		const ncoStep = (2 * Math.PI * NCO_FREQ) / rate;
		this.ncoDCos = Math.cos(ncoStep);
		this.ncoDSin = Math.sin(ncoStep);
		this.ncoRenormCounter = 0;

		this.lpI1 = new IIRFilter(IIR_A1, IIR_B1);
		this.lpQ1 = new IIRFilter(IIR_A1, IIR_B1);
		this.lpI2 = new IIRFilter(IIR_A2, IIR_B2);
		this.lpQ2 = new IIRFilter(IIR_A2, IIR_B2);
		this.decI = new Decimator(DECIMATION);
		this.decQ = new Decimator(DECIMATION);
		this.medianFilter = new MedianFilter(MEDIAN_SIZE);
		this.phaseSmooth = new IIRFilter(IIR_A_SMOOTH, IIR_B_SMOOTH);
		this.phaseDC = new IIRFilter(IIR_A2, IIR_B2);

		this.prevI = 0;
		this.prevQ = 0;
		this.prevPrevI = 0;
		this.prevPrevQ = 0;

		// AudioWorkletProcessor delivers 128 samples per process() call; pre-size
		// scratch buffers to that.
		this.iScratch = new Float64Array(128);
		this.qScratch = new Float64Array(128);
		this.freqScratch = new Int16Array(Math.ceil(128 / DECIMATION) + 16);

		this.ringBuf = new Int16Array(RING_CAPACITY);
		this.ringHead = 0;
		this.ringLen = 0;
		this.ringOffset = 0;

		this.state = 'IDLE';
		this.patternSet = null;
		this.symbolRate = 0;
		this.symbolPos = 0;
		this.symbolCount = 0;
		this.bitAccum = 0;
		this.corrAccumWeighted = 0;
		this.corrAccumWeight = 0;
		this.demodWindowLen = 0;

		this.corrMaxVals = new Array(16).fill(0);
		this.corrMinVals = new Array(16).fill(0);

		this.framingState = 'LENGTH';
		this.frameLen = 0;
		this.frameData = [];
		this.frameChecksumBytes = 0;
		this.frameChecksum = 0;
	}

	ensureScratch(blockLen) {
		if (this.iScratch.length < blockLen) {
			this.iScratch = new Float64Array(blockLen);
			this.qScratch = new Float64Array(blockLen);
		}
		const freqLen = Math.ceil(blockLen / DECIMATION) + 16;
		if (this.freqScratch.length < freqLen) {
			this.freqScratch = new Int16Array(freqLen);
		}
	}

	/**
	 * Process a block of Float32 audio samples (Web Audio native format).
	 * Samples are expected in the -1.0..1.0 range; we scale to Int16 internally.
	 */
	processBlock(samples) {
		const blockLen = samples.length;
		this.ensureScratch(blockLen);
		const iSamples = this.iScratch;
		const qSamples = this.qScratch;

		let c = this.ncoCos;
		let s = this.ncoSin;
		const dc = this.ncoDCos;
		const ds = this.ncoDSin;
		for (let n = 0; n < blockLen; n++) {
			const x = samples[n];
			iSamples[n] = x * c;
			qSamples[n] = x * -s;
			const newC = c * dc - s * ds;
			const newS = s * dc + c * ds;
			c = newC;
			s = newS;
		}
		this.ncoRenormCounter += blockLen;
		if (this.ncoRenormCounter > 10000) {
			const mag = Math.hypot(c, s);
			c /= mag;
			s /= mag;
			this.ncoRenormCounter = 0;
		}
		this.ncoCos = c;
		this.ncoSin = s;

		for (let n = 0; n < blockLen; n++) {
			iSamples[n] = this.lpI1.process(iSamples[n]);
			qSamples[n] = this.lpQ1.process(qSamples[n]);
		}

		const iDecLen = this.decI.processInto(iSamples, blockLen);
		const qDecLen = this.decQ.processInto(qSamples, blockLen);
		const len = Math.min(iDecLen, qDecLen);
		const iDec = this.decI.output;
		const qDec = this.decQ.output;

		const freqScratch = this.freqScratch;
		let freqOut = 0;
		for (let n = 0; n < len; n++) {
			const fi = this.lpI2.process(iDec[n]);
			const fq = this.lpQ2.process(qDec[n]);
			let mag = Math.sqrt(fi * fi + fq * fq);
			if (mag === 0) mag = 1e-5;

			const dI = fi - this.prevPrevI;
			const dQ = fq - this.prevPrevQ;
			const pi = this.prevI;
			const pq = this.prevQ;
			this.prevPrevI = pi;
			this.prevI = fi;
			this.prevPrevQ = pq;
			this.prevQ = fq;

			const phaseErr = (dQ * pi - dI * pq) / (mag * mag);
			const med = this.medianFilter.process(phaseErr);
			const sm = this.phaseDC.process(this.phaseSmooth.process(med));

			let val = Math.round(sm * 1000);
			if (val > CLAMP) val = CLAMP;
			if (val < -CLAMP) val = -CLAMP;
			freqScratch[freqOut++] = val;
		}

		this.appendRing(freqScratch, freqOut);
		this.processStateMachine();
	}

	appendRing(src, count) {
		if (this.ringHead + this.ringLen + count > this.ringBuf.length) {
			if (this.ringHead > 0) {
				this.ringBuf.copyWithin(0, this.ringHead, this.ringHead + this.ringLen);
				this.ringHead = 0;
			}
			if (this.ringLen + count > this.ringBuf.length) {
				const overflow = this.ringLen + count - this.ringBuf.length;
				this.ringHead = overflow;
				this.ringLen -= overflow;
				this.ringOffset += overflow;
				this.ringBuf.copyWithin(0, this.ringHead, this.ringHead + this.ringLen);
				this.ringHead = 0;
			}
		}
		const writeAt = this.ringHead + this.ringLen;
		this.ringBuf.set(src.subarray(0, count), writeAt);
		this.ringLen += count;
	}

	processStateMachine() {
		if (this.state === 'IDLE') {
			this.trySync();
		} else if (this.state === 'DECODING') {
			this.demodulate();
		}
	}

	trySync() {
		const syncLen1 = SYNC1.length;
		const needed = syncLen1 + syncLen1;
		if (this.ringLen < needed) return;

		const buf = this.ringBuf.subarray(this.ringHead, this.ringHead + this.ringLen);
		const corr1 = correlate(buf, SYNC1, 0, buf.length);

		if (corr1.maxVal >= SYNC_THRESHOLD && Math.abs(corr1.maxVal) > Math.abs(corr1.minVal)) {
			if (corr1.maxIdx > buf.length - syncLen1 - 10) {
				this.consumeRing(Math.floor(syncLen1 / 2));
				return;
			}
			this.patternSet = PATTERN_SET_1;
			this.onSyncFound(corr1);
			return;
		}

		if (Math.abs(corr1.minVal) >= SYNC_THRESHOLD) {
			const corr2 = correlate(buf, SYNC2, 0, buf.length);
			this.patternSet = PATTERN_SET_2;
			this.onSyncFound(corr2);
			return;
		}

		this.consumeRing(syncLen1);
	}

	onSyncFound(corr) {
		this.state = 'DECODING';
		const ps = this.patternSet;

		this.onSync({
			quality: Math.round((corr.maxVal * 100) / (CLAMP * ps.sync.length)),
			pattern: ps.id
		});
		this.onLog(`Sync found (Pattern ${ps.id}, Correlation: ${corr.maxVal})`);

		this.symbolRate = ps.symbolLen;
		const bitPatLen = BIT_PATTERN.length;
		this.demodWindowLen = bitPatLen + SYMBOL_CORRELATOR_EXTRA * 2;

		const syncIdx = corr.maxVal >= Math.abs(corr.minVal) ? corr.maxIdx : corr.minIdx;
		const baseSyncLen = ps.sync.length;

		const dataStart = syncIdx + baseSyncLen;
		this.symbolPos = this.ringOffset + dataStart;
		this.symbolCount = 0;
		this.bitAccum = 0;
		this.corrAccumWeighted = 0;
		this.corrAccumWeight = 0;

		this.framingState = 'LENGTH';
		this.frameLen = 0;
		this.frameData = [];
		this.frameChecksumBytes = 0;
		this.frameChecksum = 0;
	}

	demodulate() {
		const bitPatLen = BIT_PATTERN.length;
		const windowLen = this.demodWindowLen;

		while (true) {
			const readStart = Math.floor(this.symbolPos) - this.ringOffset - SYMBOL_CORRELATOR_EXTRA;
			if (readStart < 0) return;
			if (readStart + windowLen > this.ringLen) return;

			const window = this.ringBuf.subarray(
				this.ringHead + readStart,
				this.ringHead + readStart + windowLen
			);

			const c = correlate(window, BIT_PATTERN, 0, windowLen);

			const bitIdx = this.symbolCount % 16;
			this.corrMaxVals[bitIdx] = c.maxVal;
			this.corrMinVals[bitIdx] = c.minVal;

			if (c.maxVal > -c.minVal) {
				this.bitAccum |= 1 << bitIdx;
				const w = c.maxVal;
				this.corrAccumWeighted += (c.maxIdx - SYMBOL_CORRELATOR_EXTRA) * w;
				this.corrAccumWeight += w;
			} else {
				this.bitAccum &= ~(1 << bitIdx);
				const w = -c.minVal;
				this.corrAccumWeighted += (c.minIdx - SYMBOL_CORRELATOR_EXTRA) * w;
				this.corrAccumWeight += w;
			}

			if (bitIdx >= 15 && this.symbolCount > 0) {
				const lowByte = this.bitAccum & 0xff;
				const highByte = (this.bitAccum >> 8) & 0xff;
				const dec1 = hammingDecode(lowByte);
				const dec2 = hammingDecode(highByte);
				const charVal = (dec1.value & 0xf) | ((dec2.value & 0xf) << 4);

				if (this.corrAccumWeight > 0) {
					const adj = this.corrAccumWeighted / this.corrAccumWeight;
					this.symbolPos += adj;
				}
				this.corrAccumWeighted = 0;
				this.corrAccumWeight = 0;
				this.bitAccum = 0;

				this.processFramingChar(charVal);
			}

			const consumed = Math.floor(this.symbolPos) - this.ringOffset;
			if (consumed > bitPatLen && consumed < this.ringLen) {
				this.consumeRing(consumed - bitPatLen);
			}

			this.symbolCount++;
			this.symbolPos += this.symbolRate;
		}
	}

	processFramingChar(charVal) {
		if (this.framingState === 'LENGTH') {
			if (charVal >= 40) {
				this.onLog('Error: Invalid data length ' + charVal);
				this.resetToIdle();
				return;
			}
			this.frameLen = charVal;
			this.frameData = [];
			this.framingState = 'DATA';
			this.onLog('Frame length: ' + charVal + ' bytes');
		} else if (this.framingState === 'DATA') {
			this.frameData.push(charVal);
			if (this.frameData.length >= this.frameLen) {
				this.framingState = 'CHECKSUM';
				this.frameChecksumBytes = 0;
				this.frameChecksum = 0;
			}
		} else if (this.framingState === 'CHECKSUM') {
			if (this.frameChecksumBytes === 0) {
				this.frameChecksum = charVal;
				this.frameChecksumBytes = 1;
			} else {
				this.frameChecksum |= charVal << 8;

				const computed = crc16(this.frameData, this.frameLen);
				if (computed !== this.frameChecksum) {
					this.onLog(
						`CRC error: computed=${computed.toString(16)}, received=${this.frameChecksum.toString(16)}`
					);
					this.onError('CRC checksum error');
					this.resetToIdle();
					return;
				}

				this.onLog('CRC OK ✓');
				const hexBytes = this.frameData.map((b) => b.toString(16).padStart(2, '0'));
				this.onLog('Data: ' + hexBytes.join(' '));
				this.onFrame(hexBytes);
				this.resetToIdle();
			}
		}
	}

	consumeRing(count) {
		if (count <= 0 || count > this.ringLen) return;
		this.ringHead += count;
		this.ringLen -= count;
		this.ringOffset += count;
	}

	resetToIdle() {
		this.state = 'IDLE';
		this.ringHead = 0;
		this.ringLen = 0;
		this.ringOffset = 0;
	}
}

// ========================
// AudioWorkletProcessor wrapper
// ========================

class TunerProcessor extends AudioWorkletProcessor {
	constructor(options) {
		super();
		const opts = options && options.processorOptions ? options.processorOptions : {};
		this.targetRate = opts.sampleRate || SAMPLE_RATE;

		// Throttle level + log postMessage rate. AudioWorkletProcessor.process()
		// runs every 128 samples → ~344 Hz @ 44.1k. Posting at that rate would
		// drown the main thread; we sample at ~20 Hz instead.
		this.lastLevelPost = 0;
		this.maxAbsThisWindow = 0;

		try {
			this.pipeline = new TunerPipeline(
				{
					onLog: (msg) => this.port.postMessage({ type: 'log', message: msg }),
					onSync: (info) => this.port.postMessage({ type: 'sync', info }),
					onFrame: (hexBytes) => this.port.postMessage({ type: 'frame', hexBytes }),
					onError: (msg) => this.port.postMessage({ type: 'error', message: msg })
				},
				this.targetRate
			);
		} catch (e) {
			this.port.postMessage({ type: 'error', message: e.message });
			this.pipeline = null;
		}
	}

	process(inputs) {
		if (!this.pipeline) return false; // shut down
		const input = inputs[0];
		if (!input || input.length === 0) return true;
		const channel = input[0];
		if (!channel || channel.length === 0) return true;

		// Level meter - peak over the block. Posted at ~20 Hz to keep the
		// main-thread message queue light.
		let maxAbs = 0;
		for (let i = 0; i < channel.length; i++) {
			const a = Math.abs(channel[i]);
			if (a > maxAbs) maxAbs = a;
		}
		if (maxAbs > this.maxAbsThisWindow) this.maxAbsThisWindow = maxAbs;
		// `currentTime` is a globally-defined property in AudioWorkletGlobalScope.
		if (currentTime - this.lastLevelPost > 0.05) {
			this.port.postMessage({ type: 'level', value: this.maxAbsThisWindow });
			this.maxAbsThisWindow = 0;
			this.lastLevelPost = currentTime;
		}

		this.pipeline.processBlock(channel);
		return true;
	}
}

registerProcessor('tuner-processor', TunerProcessor);
