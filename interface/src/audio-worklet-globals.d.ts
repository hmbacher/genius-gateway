// Minimal AudioWorkletGlobalScope declarations (not part of TypeScript's DOM lib),
// needed to type-check src/lib/audio/tuner-worklet.js.
declare const currentTime: number;
declare function registerProcessor(name: string, processorCtor: unknown): void;
declare class AudioWorkletProcessor {
	readonly port: MessagePort;
	constructor(options?: unknown);
}
