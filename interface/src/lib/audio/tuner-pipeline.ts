/**
 * Hekatron SmartSonic Decoder - main-thread session manager.
 *
 * The signal-processing pipeline runs in an AudioWorklet (tuner-worklet.js)
 * on the dedicated audio rendering thread; this file orchestrates the worklet
 * lifecycle and converts its messages into the UI-friendly callbacks the
 * caller subscribes to.
 */

import { parseTunerData } from './tuner-parser';
import type { SyncInfo, TunerCallbacks, TunerData, TunerState } from './tuner-types';

// The worklet file is plain JS with no imports (so AudioWorkletGlobalScope
// can load it directly). `?url` asks Vite to copy it into the build output
// and hand us back its URL; in dev it serves the file as-is.
import workletUrl from './tuner-worklet.js?url';

export type { SyncInfo, TunerCallbacks, TunerData, TunerState };

const SAMPLE_RATE = 44100;

/** Hard cap for time spent waiting on the very first sync. */
const TIMEOUT_MS = 60000;
/** Hard cap for time spent in the decoding state without producing a valid frame. */
const DECODE_STUCK_MS = 30000;

type WorkletMessage =
	| { type: 'log'; message: string }
	| { type: 'sync'; info: SyncInfo }
	| { type: 'frame'; hexBytes: string[] }
	| { type: 'error'; message: string }
	| { type: 'level'; value: number };

export class AcousticDetectionSession {
	private audioContext: AudioContext | null = null;
	private mediaStream: MediaStream | null = null;
	private workletNode: AudioWorkletNode | null = null;
	private silenceTimer: ReturnType<typeof setTimeout> | null = null;
	private stuckTimer: ReturnType<typeof setTimeout> | null = null;
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

			// IIR coefficients are baked for SAMPLE_RATE; fail hard on a mismatch.
			if (audioContext.sampleRate !== SAMPLE_RATE) {
				this.callbacks.onError(
					`Browser delivered ${audioContext.sampleRate} Hz, but the decoder requires ${SAMPLE_RATE} Hz.`
				);
				this.setState('error');
				this.stop();
				return;
			}

			await audioContext.audioWorklet.addModule(workletUrl);

			const source = audioContext.createMediaStreamSource(mediaStream);
			const node = new AudioWorkletNode(audioContext, 'tuner-processor', {
				numberOfInputs: 1,
				numberOfOutputs: 1,
				outputChannelCount: [1],
				processorOptions: { sampleRate: audioContext.sampleRate }
			});
			this.workletNode = node;

			node.port.onmessage = (ev: MessageEvent<WorkletMessage>) => this.onWorkletMessage(ev.data);

			source.connect(node);
			// Connect to destination so the worklet's process() keeps getting called.
			// We don't actually want to hear anything; mute the chain on the way out.
			const muteGain = audioContext.createGain();
			muteGain.gain.value = 0;
			node.connect(muteGain).connect(audioContext.destination);

			this.callbacks.onLog(`Recording started (${audioContext.sampleRate} Hz, Mono)`);

			this.silenceTimer = setTimeout(() => {
				if (this._state === 'waiting') {
					this.callbacks.onError('Timeout - no signal detected');
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
				this.callbacks.onError('Microphone access failed: ' + (error.message ?? String(err)));
			}
			this.setState('error');
		}
	}

	private onWorkletMessage(msg: WorkletMessage): void {
		switch (msg.type) {
			case 'log':
				this.callbacks.onLog(msg.message);
				break;

			case 'level':
				this.callbacks.onLevelUpdate(msg.value);
				break;

			case 'sync':
				this.setState('synced');
				this.callbacks.onSync(msg.info);
				// Successful sync - silence timer no longer relevant; arm the
				// stuck-decoding timer so we don't sit in 'decoding' forever.
				this.clearSilenceTimer();
				this.armStuckTimer();
				setTimeout(() => {
					if (this._state === 'synced') this.setState('decoding');
				}, 500);
				break;

			case 'frame': {
				try {
					const data = parseTunerData(msg.hexBytes);
					this.clearStuckTimer();
					this.setState('success');
					this.callbacks.onData(data);
					setTimeout(() => this.stop(), 500);
				} catch (e) {
					this.callbacks.onError('Parsing error: ' + (e as Error).message);
					this.setState('error');
				}
				break;
			}

			case 'error':
				this.clearStuckTimer();
				this.setState('error');
				this.callbacks.onError(msg.message);
				break;
		}
	}

	private clearSilenceTimer(): void {
		if (this.silenceTimer) {
			clearTimeout(this.silenceTimer);
			this.silenceTimer = null;
		}
	}

	private clearStuckTimer(): void {
		if (this.stuckTimer) {
			clearTimeout(this.stuckTimer);
			this.stuckTimer = null;
		}
	}

	private armStuckTimer(): void {
		this.clearStuckTimer();
		this.stuckTimer = setTimeout(() => {
			if (this._state === 'synced' || this._state === 'decoding') {
				this.callbacks.onError('Stuck while decoding - no valid frame received.');
				this.setState('error');
				this.stop();
			}
		}, DECODE_STUCK_MS);
	}

	stop(): void {
		this.clearSilenceTimer();
		this.clearStuckTimer();
		if (this.workletNode) {
			this.workletNode.port.onmessage = null;
			this.workletNode.disconnect();
			this.workletNode = null;
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
