// ============================================================
// Hekatron SmartSonic Decoder — TypeScript Port
// Ported from reverse-engineered Android APK (Tuner package)
// ============================================================

// --- Configuration ---
const SAMPLE_RATE = 44100;
const NCO_FREQ = 4449.1;
const DECIMATION = 8;
const SYNC_THRESHOLD = 300000;
const SYMBOL_CORRELATOR_EXTRA = 12;
const CLAMP = 400;

// --- IIR Filter coefficients ---
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
function buildBitPattern(): Int16Array {
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

// --- Sync patterns ---
function buildSyncPattern1(): Int16Array {
	const segs: [number, number][] = [
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
	];
	const p = new Int16Array(1576);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}
function buildSyncPattern2(): Int16Array {
	const segs: [number, number][] = [
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
	];
	const p = new Int16Array(1578);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}

function buildSubSync1g(): Int16Array {
	const segs: [number, number][] = [
		[65, -1],
		[33, 1],
		[33, -1],
		[66, 1]
	];
	const p = new Int16Array(197);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}
function buildSubSync1h(): Int16Array {
	const segs: [number, number][] = [
		[65, -1],
		[32, 1],
		[33, -1],
		[67, 1]
	];
	const p = new Int16Array(197);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}
function buildSubSync1i(): Int16Array {
	const segs: [number, number][] = [
		[33, -1],
		[33, 1],
		[33, -1],
		[32, 1],
		[33, -1],
		[33, 1]
	];
	const p = new Int16Array(197);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}

function buildSubSync2g(): Int16Array {
	const segs: [number, number][] = [
		[67, 1],
		[32, -1],
		[34, 1],
		[64, -1]
	];
	const p = new Int16Array(197);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}
function buildSubSync2h(): Int16Array {
	const segs: [number, number][] = [
		[67, 1],
		[33, -1],
		[33, 1],
		[64, -1]
	];
	const p = new Int16Array(197);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}
function buildSubSync2i(): Int16Array {
	const segs: [number, number][] = [
		[34, 1],
		[32, -1],
		[34, 1],
		[32, -1],
		[33, 1],
		[32, -1]
	];
	const p = new Int16Array(197);
	let idx = 0;
	for (const [len, val] of segs) for (let i = 0; i < len; i++) p[idx++] = val;
	return p;
}

const SYNC1 = buildSyncPattern1();
const SYNC2 = buildSyncPattern2();

interface PatternSet {
	id: number;
	sync: Int16Array;
	g: Int16Array;
	h: Int16Array;
	i: Int16Array;
	bOffset: number;
	cOffset: number;
	dOffset: number;
	symbolLen: number;
}

const PATTERN_SET_1: PatternSet = {
	id: 1,
	sync: SYNC1,
	g: buildSubSync1g(),
	h: buildSubSync1h(),
	i: buildSubSync1i(),
	bOffset: 133,
	cOffset: 1250,
	dOffset: 690,
	symbolLen: 65.708
};

const PATTERN_SET_2: PatternSet = {
	id: 2,
	sync: SYNC2,
	g: buildSubSync2g(),
	h: buildSubSync2h(),
	i: buildSubSync2i(),
	bOffset: 130,
	cOffset: 1250,
	dOffset: 690,
	symbolLen: 65.802
};

// ========================
// DSP building blocks
// ========================

class IIRFilter {
	private a: number[];
	private b: number[];
	private state: Float64Array;
	private order: number;

	constructor(a: number[], b: number[]) {
		this.a = a;
		this.b = b;
		this.state = new Float64Array(a.length).fill(0);
		this.order = a.length - 1;
	}

	process(x: number): number {
		let y = this.b[0] * x + this.state[1];
		for (let i = 1; i < this.order; i++) {
			this.state[i] = this.state[i + 1] + this.b[i] * x - this.a[i] * y;
		}
		this.state[this.order] = this.b[this.order] * x - this.a[this.order] * y;
		return y;
	}
}

class MedianFilter {
	private size: number;
	private buf: Float64Array;
	private sorted: Float64Array;
	private odd: boolean;

	constructor(size: number) {
		this.size = size;
		this.buf = new Float64Array(size).fill(0);
		this.sorted = new Float64Array(size).fill(0);
		this.odd = size % 2 !== 0;
	}

	process(x: number): number {
		for (let i = this.size - 1; i > 0; i--) {
			this.buf[i] = this.buf[i - 1];
			this.sorted[i] = this.buf[i - 1];
		}
		this.buf[0] = x;
		this.sorted[0] = x;
		this.sorted.sort();
		if (this.odd) return this.sorted[Math.floor(this.size / 2)];
		const h = this.size / 2;
		return (this.sorted[h] + this.sorted[h + 1]) / 2;
	}
}

class Decimator {
	private factor: number;
	private counter: number;

	constructor(factor: number) {
		this.factor = factor;
		this.counter = 0;
	}

	process(arr: Float64Array): number[] {
		const out: number[] = [];
		for (let i = 0; i < arr.length; i++) {
			this.counter++;
			if (this.counter >= this.factor) {
				this.counter = 0;
				out.push(arr[i]);
			}
		}
		return out;
	}
}

function correlate(
	signal: Int16Array,
	pattern: Int16Array,
	start: number,
	windowLen: number
): { maxVal: number; maxIdx: number; minVal: number; minIdx: number } {
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

function hammingDist(a: number, b: number): number {
	let d = a ^ b,
		c = 0;
	for (let i = 0; i < 8; i++) if ((d >> i) & 1) c++;
	return c;
}

function hammingDecode(raw: number): { value: number; errors: number } {
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
// CRC16
// ========================
function crc16(data: number[], len: number): number {
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
// Data parser
// ========================
export interface TunerData {
	rawHex: string;
	protocolVersion: number;
	serialNumber: number;
	product: string;
	productType: number;
	radioProduct: string;
	radioProductType: number;
	deinstallationCount: number;
	alarmCount: number;
	alarmCountLast3Months: number;
	productionDate: Date;
	lastAlarm: Date | null;
	hoursInStorageMode: number;
	lastSelftest: Date | null;
	warrantyFlags: string[];
	warrantyFlagsRaw: number;
	batteryLowFault: boolean;
	deviceFault: boolean;
	radioNetworkFault: boolean;
	driftState: number;
	dirtForecastNegative: boolean;
	hasRadio: boolean;
	radioState?: string[];
	radioStateMask?: number;
	radioSerialNumber?: number;
	lineId?: number;
	lineCharacter?: string;
	lineNumber?: number;
	radioSwitchFlags?: string[];
	radioSwitchMask?: number;
	radioInterference?: number;
}

function parseTunerData(hexBytes: string[]): TunerData {
	if (hexBytes.length < 20) throw new Error('Insufficient data: ' + hexBytes.length + ' bytes');

	const b = hexBytes.map((h) => parseInt(h, 16));
	const result: Partial<TunerData> = {};
	result.rawHex = hexBytes.join(' ');

	result.protocolVersion = b[0];
	result.serialNumber = parseInt(hexBytes[1] + hexBytes[2] + hexBytes[3] + hexBytes[4], 16);

	const prodType = b[5] & 0x0f;
	result.productType = prodType;
	result.product =
		['Genius H', 'Genius Hx', 'Genius Plus', 'Genius Plus X'][prodType] || 'unknown';

	const radioProd = (b[5] & 0xf0) >> 4;
	result.radioProductType = radioProd;
	result.radioProduct =
		['no FM', 'FM.Basis', 'FM.Pro', 'FM.MCP', 'FM.Basis X', 'FM.Pro X'][radioProd] || 'unknown';

	result.deinstallationCount = b[6];
	result.alarmCount = b[7];
	result.alarmCountLast3Months = b[8];

	const productionAge = parseInt(hexBytes[12] + hexBytes[11], 16);
	const lastAlarmOffset = parseInt(hexBytes[10] + hexBytes[9], 16);
	const now = new Date();

	result.productionDate = new Date(now.getTime() - productionAge * 86400000);
	if (lastAlarmOffset !== 0xffff && productionAge - lastAlarmOffset >= 0) {
		result.lastAlarm = new Date(now.getTime() - (productionAge - lastAlarmOffset) * 86400000);
	} else {
		result.lastAlarm = null;
	}

	result.hoursInStorageMode = parseInt(hexBytes[14] + hexBytes[13], 16);

	const lastSelftestOffset = parseInt(hexBytes[16] + hexBytes[15], 16);
	if (lastSelftestOffset !== 0xffff && productionAge - lastSelftestOffset >= 0) {
		result.lastSelftest = new Date(
			now.getTime() - (productionAge - lastSelftestOffset) * 86400000
		);
	} else {
		result.lastSelftest = null;
	}

	const warrantyRaw = parseInt(hexBytes[18] + hexBytes[17], 16);
	result.warrantyFlagsRaw = warrantyRaw;
	result.warrantyFlags = [];
	const WARRANTY_NAMES = [
		'MaxDirty',
		'OutOfTemp',
		'DetectorTooOld',
		'StorageTimeExceeded',
		'ActivationTimeExceeded',
		'TooManyEvents',
		'TooManyAlarms',
		'TooManyFaults',
		'TooManySelfTests',
		'TooManyRadioFaults',
		'TooManyRadioOutOfOrderEvents',
		'RadioInstallationTooOld',
		'TooMuchRadioActivity',
		'TooMuchRadioInterference',
		'TooManyRadioTxEvents',
		'TooManyRadioRxEvents'
	];
	if (warrantyRaw === 0) {
		result.warrantyFlags.push('WarrantyPossible');
	} else {
		for (let i = 0; i < 16; i++) {
			if ((warrantyRaw >> i) & 1) result.warrantyFlags.push(WARRANTY_NAMES[i]);
		}
	}

	const detByte = b[19];
	result.batteryLowFault = !!(detByte & 0x01);
	result.deviceFault = !!(detByte & 0x02);
	result.radioNetworkFault = !!(detByte & 0x04);
	result.driftState = (detByte >> 3) & 0x0f;
	result.dirtForecastNegative = !!(detByte & 0x80);

	if (hexBytes.length > 20) {
		result.hasRadio = true;
		const radioStateByte = b[20];
		result.radioStateMask = radioStateByte;
		result.radioState = [];
		const RADIO_NAMES = [
			'FmFault',
			'TransmissionRangeTest',
			'Selftest',
			'FmBatteryLowFault',
			'RemoteBattLow',
			'RemoteError',
			'RadioLinkError',
			'RemoteAlarm'
		];
		for (let i = 0; i < 8; i++) {
			if ((radioStateByte >> i) & 1) result.radioState.push(RADIO_NAMES[i]);
		}
		result.radioSerialNumber = parseInt(
			hexBytes[21] + hexBytes[22] + hexBytes[23] + hexBytes[24],
			16
		);
		result.lineId = parseInt(hexBytes[25] + hexBytes[26] + hexBytes[27] + hexBytes[28], 16);

		const lineByte = b[29];
		const lineCharIdx = (lineByte & 0xf0) >> 4;
		result.lineCharacter = 'ABCDEFGHIJ'[lineCharIdx] || '?';
		result.lineNumber = lineByte & 0x0f;

		const switchByte = b[30];
		result.radioSwitchMask = switchByte;
		result.radioSwitchFlags = [];
		const SWITCH_MAP: [number, string][] = [
			[2, 'ReducedTransmittingPower'],
			[3, 'RadioLinkSupervision'],
			[4, 'ReceiveCollectiveAlarm'],
			[5, 'SendCollectiveAlarm'],
			[6, 'SuppressAlarms'],
			[7, 'SuppressWarnings']
		];
		for (const [bit, name] of SWITCH_MAP) {
			if ((switchByte >> bit) & 1) result.radioSwitchFlags.push(name);
		}
		result.radioInterference = b[31] > 0 ? b[31] / 10.0 : b[31];
	} else {
		result.hasRadio = false;
	}

	return result as TunerData;
}

// ========================
// Exported types and events
// ========================

export type TunerState = 'idle' | 'waiting' | 'synced' | 'decoding' | 'success' | 'error';

export interface SyncInfo {
	quality: number;
	pattern: number;
}

export interface TunerCallbacks {
	onLog: (msg: string) => void;
	onSync: (info: SyncInfo) => void;
	onData: (data: TunerData) => void;
	onError: (msg: string) => void;
	onLevelUpdate: (level: number) => void;
}

// ========================
// Signal processing pipeline
// ========================

class TunerPipeline {
	private onLog: (msg: string) => void;
	private onSync: (info: SyncInfo) => void;
	private onData: (data: TunerData) => void;
	private onError: (msg: string) => void;

	private ncoStep: number;
	private ncoPhase = 0;

	private lpI1: IIRFilter;
	private lpQ1: IIRFilter;
	private lpI2: IIRFilter;
	private lpQ2: IIRFilter;
	private decI: Decimator;
	private decQ: Decimator;
	private medianFilter: MedianFilter;
	private phaseSmooth: IIRFilter;
	private phaseDC: IIRFilter;

	private prevI = 0;
	private prevQ = 0;
	private prevPrevI = 0;
	private prevPrevQ = 0;

	private ringBuf: number[] = [];
	private ringOffset = 0;

	private state: 'IDLE' | 'SYNCED' | 'DECODING' = 'IDLE';
	private patternSet: PatternSet | null = null;
	private symbolRate = 0;
	private symbolPos = 0;
	private symbolCount = 0;
	private bitAccum = 0;
	private corrAccumWeighted = 0;
	private corrAccumWeight = 0;
	private demodWindowLen = 0;

	private corrMaxVals = new Array(16).fill(0);
	private corrMinVals = new Array(16).fill(0);

	private framingState: 'LENGTH' | 'DATA' | 'CHECKSUM' = 'LENGTH';
	private frameLen = 0;
	private frameData: number[] = [];
	private frameChecksumBytes = 0;
	private frameChecksum = 0;

	private lastSyncTime = 0;
	public dataReceived = false;

	constructor(callbacks: Omit<TunerCallbacks, 'onLevelUpdate'>, sampleRate?: number) {
		this.onLog = callbacks.onLog;
		this.onSync = callbacks.onSync;
		this.onData = callbacks.onData;
		this.onError = callbacks.onError;
		this.ncoStep = 27954.519750172698 / (sampleRate || SAMPLE_RATE);

		this.lpI1 = new IIRFilter(IIR_A1, IIR_B1);
		this.lpQ1 = new IIRFilter(IIR_A1, IIR_B1);
		this.lpI2 = new IIRFilter(IIR_A2, IIR_B2);
		this.lpQ2 = new IIRFilter(IIR_A2, IIR_B2);
		this.decI = new Decimator(DECIMATION);
		this.decQ = new Decimator(DECIMATION);
		this.medianFilter = new MedianFilter(MEDIAN_SIZE);
		this.phaseSmooth = new IIRFilter(IIR_A_SMOOTH, IIR_B_SMOOTH);
		this.phaseDC = new IIRFilter(IIR_A2, IIR_B2);
	}

	processBlock(samples: Int16Array): void {
		const iSamples = new Float64Array(samples.length);
		const qSamples = new Float64Array(samples.length);
		for (let n = 0; n < samples.length; n++) {
			const s = samples[n] / 32768.0;
			iSamples[n] = s * Math.cos(this.ncoPhase);
			qSamples[n] = s * -Math.sin(this.ncoPhase);
			this.ncoPhase = (this.ncoPhase + this.ncoStep) % (2 * Math.PI);
		}

		for (let n = 0; n < samples.length; n++) {
			iSamples[n] = this.lpI1.process(iSamples[n]);
			qSamples[n] = this.lpQ1.process(qSamples[n]);
		}

		const iDec = this.decI.process(iSamples);
		const qDec = this.decQ.process(qSamples);

		const len = Math.min(iDec.length, qDec.length);
		const freqSamples: number[] = [];
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
			freqSamples.push(val);
		}

		for (const v of freqSamples) this.ringBuf.push(v);
		this.processStateMachine();
	}

	private processStateMachine(): void {
		if (this.state === 'IDLE') {
			this.trySync();
		} else if (this.state === 'DECODING') {
			this.demodulate();
		}
	}

	private trySync(): void {
		const syncLen1 = SYNC1.length;
		const needed = syncLen1 + syncLen1;
		if (this.ringBuf.length < needed) return;

		const buf = new Int16Array(this.ringBuf.length);
		for (let i = 0; i < this.ringBuf.length; i++) buf[i] = this.ringBuf[i];

		const corr1 = correlate(buf, SYNC1, 0, buf.length);

		if (corr1.maxVal >= SYNC_THRESHOLD && Math.abs(corr1.maxVal) > Math.abs(corr1.minVal)) {
			if (corr1.maxIdx > buf.length - syncLen1 - 10) {
				this.consumeRing(Math.floor(syncLen1 / 2));
				return;
			}
			this.patternSet = PATTERN_SET_1;
			this.onSyncFound(corr1, buf);
			return;
		}

		if (Math.abs(corr1.minVal) >= SYNC_THRESHOLD) {
			const corr2 = correlate(buf, SYNC2, 0, buf.length);
			this.patternSet = PATTERN_SET_2;
			this.onSyncFound(corr2, buf);
			return;
		}

		this.consumeRing(syncLen1);
	}

	private onSyncFound(
		corr: { maxVal: number; maxIdx: number; minVal: number; minIdx: number },
		_buf: Int16Array
	): void {
		this.state = 'DECODING';
		this.lastSyncTime = Date.now();
		this.dataReceived = false;
		const ps = this.patternSet!;

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

	private demodulate(): void {
		const bitPatLen = BIT_PATTERN.length;
		const windowLen = this.demodWindowLen;

		while (true) {
			const readStart =
				Math.floor(this.symbolPos) - this.ringOffset - SYMBOL_CORRELATOR_EXTRA;
			if (readStart < 0) return;
			if (readStart + windowLen > this.ringBuf.length) return;

			const window = new Int16Array(windowLen);
			for (let i = 0; i < windowLen; i++) window[i] = this.ringBuf[readStart + i];

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
			if (consumed > bitPatLen && consumed < this.ringBuf.length) {
				this.consumeRing(consumed - bitPatLen);
			}

			this.symbolCount++;
			this.symbolPos += this.symbolRate;
		}
	}

	private processFramingChar(charVal: number): void {
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

				try {
					const data = parseTunerData(hexBytes);
					this.dataReceived = true;
					this.onData(data);
				} catch (e) {
					this.onError('Parsing error: ' + (e as Error).message);
				}
				this.resetToIdle();
			}
		}
	}

	private consumeRing(count: number): void {
		if (count <= 0 || count > this.ringBuf.length) return;
		this.ringBuf.splice(0, count);
		this.ringOffset += count;
	}

	private resetToIdle(): void {
		this.state = 'IDLE';
		this.ringBuf = [];
		this.ringOffset = 0;
	}
}

// ========================
// Audio session manager
// ========================

const TIMEOUT_MS = 60000;

export class AcousticDetectionSession {
	private audioContext: AudioContext | null = null;
	private mediaStream: MediaStream | null = null;
	private pipeline: TunerPipeline | null = null;
	private silenceTimer: ReturnType<typeof setTimeout> | null = null;
	private callbacks: TunerCallbacks;
	private _state: TunerState = 'idle';
	private onStateChange: (state: TunerState) => void;

	constructor(callbacks: TunerCallbacks, onStateChange: (state: TunerState) => void) {
		this.callbacks = callbacks;
		this.onStateChange = onStateChange;
	}

	get state(): TunerState {
		return this._state;
	}

	private setState(s: TunerState): void {
		this._state = s;
		this.onStateChange(s);
	}

	async start(): Promise<void> {
		this.setState('waiting');

		try {
			const mediaStream = await navigator.mediaDevices.getUserMedia({
				audio: {
					sampleRate: SAMPLE_RATE,
					echoCancellation: false,
					noiseSuppression: false,
					autoGainControl: false,
					channelCount: 1
				}
			});
			this.mediaStream = mediaStream;

			const audioContext = new AudioContext({ sampleRate: SAMPLE_RATE });
			this.audioContext = audioContext;

			if (audioContext.sampleRate !== SAMPLE_RATE) {
				this.callbacks.onLog(
					`Warning: Browser uses ${audioContext.sampleRate} Hz instead of ${SAMPLE_RATE} Hz`
				);
			}

			const actualRate = audioContext.sampleRate;
			const source = audioContext.createMediaStreamSource(mediaStream);
			const bufferSize = 4096;
			const processor = audioContext.createScriptProcessor(bufferSize, 1, 1);

			this.pipeline = new TunerPipeline(
				{
					onLog: (msg) => this.callbacks.onLog(msg),
					onSync: (info) => {
						this.setState('synced');
						this.callbacks.onSync(info);
						// Transition to decoding state after brief sync display
						setTimeout(() => {
							if (this._state === 'synced') this.setState('decoding');
						}, 500);
					},
					onData: (data) => {
						this.setState('success');
						this.callbacks.onData(data);
						setTimeout(() => this.stop(), 500);
					},
					onError: (msg) => {
						this.setState('error');
						this.callbacks.onError(msg);
					}
				},
				actualRate
			);

			processor.onaudioprocess = (e: AudioProcessingEvent) => {
				const input = e.inputBuffer.getChannelData(0);
				const int16 = new Int16Array(input.length);
				for (let i = 0; i < input.length; i++) {
					int16[i] = Math.round(input[i] * 32768);
				}

				// Level meter
				let maxAbs = 0;
				for (let i = 0; i < input.length; i += 64) {
					const a = Math.abs(input[i]);
					if (a > maxAbs) maxAbs = a;
				}
				this.callbacks.onLevelUpdate(maxAbs);

				this.pipeline!.processBlock(int16);
			};

			source.connect(processor);
			processor.connect(audioContext.destination);

			this.callbacks.onLog('Recording started (44100 Hz, Mono)');

			// Timeout
			this.silenceTimer = setTimeout(() => {
				if (this._state === 'waiting') {
					this.callbacks.onError('Timeout — no signal detected');
					this.setState('error');
					this.stop();
				}
			}, TIMEOUT_MS);
		} catch (err: unknown) {
			const error = err as DOMException;
			if (error.name === 'NotFoundError') {
				this.callbacks.onError('No microphone found.');
			} else if (error.name === 'NotAllowedError') {
				this.callbacks.onError('Microphone permission denied.');
			} else {
				this.callbacks.onError('Microphone access failed: ' + error.message);
			}
			this.setState('error');
		}
	}

	stop(): void {
		if (this.silenceTimer) {
			clearTimeout(this.silenceTimer);
			this.silenceTimer = null;
		}
		if (this.audioContext) {
			this.audioContext.close();
			this.audioContext = null;
		}
		if (this.mediaStream) {
			this.mediaStream.getTracks().forEach((t) => t.stop());
			this.mediaStream = null;
		}
		this.callbacks.onLog('Recording stopped.');
	}
}
