/**
 * Types shared between the main-thread session manager and consumers of the
 * acoustic detection pipeline. The DSP itself runs in tuner-worklet.js.
 */

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
