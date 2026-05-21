import type { TunerData } from './tuner-types';

/**
 * Decodes a verified frame (already CRC-checked by the worklet) into a
 * structured TunerData object. Pure function so it can be unit-tested
 * without a Web Audio context.
 */
export function parseTunerData(hexBytes: string[]): TunerData {
	if (hexBytes.length < 20) throw new Error('Insufficient data: ' + hexBytes.length + ' bytes');

	const b = hexBytes.map((h) => parseInt(h, 16));
	const result: Partial<TunerData> = {};
	result.rawHex = hexBytes.join(' ');

	result.protocolVersion = b[0];
	result.serialNumber = parseInt(hexBytes[1] + hexBytes[2] + hexBytes[3] + hexBytes[4], 16);

	const prodType = b[5] & 0x0f;
	result.productType = prodType;
	result.product = ['Genius H', 'Genius Hx', 'Genius Plus', 'Genius Plus X'][prodType] || 'unknown';

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
		result.lastSelftest = new Date(now.getTime() - (productionAge - lastSelftestOffset) * 86400000);
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
