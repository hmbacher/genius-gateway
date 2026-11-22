export type WifiStatus = {
	status: number;
	local_ip: string;
	mac_address: string;
	rssi: number;
	ssid: string;
	bssid: string;
	channel: number;
	subnet_mask: string;
	gateway_ip: string;
	dns_ip_1: string;
	dns_ip_2?: string;
};

export type WifiSettings = {
	hostname: string;
	connection_mode: number;
	wifi_networks: KnownNetworkItem[];
};

export type KnownNetworkItem = {
	ssid: string;
	password: string;
	static_ip_config: boolean;
	local_ip?: string;
	subnet_mask?: string;
	gateway_ip?: string;
	dns_ip_1?: string;
	dns_ip_2?: string;
};

export type NetworkItem = {
	rssi: number;
	ssid: string;
	bssid: string;
	channel: number;
	encryption_type: number;
};

export type ApStatus = {
	status: number;
	ip_address: string;
	mac_address: string;
	station_num: number;
};

export type ApSettings = {
	provision_mode: number;
	ssid: string;
	password: string;
	channel: number;
	ssid_hidden: boolean;
	max_clients: number;
	local_ip: string;
	gateway_ip: string;
	subnet_mask: string;
};

export type LightState = {
	led_on: boolean;
};

export type BrokerSettings = {
	mqtt_path: string;
	name: string;
	unique_id: string;
};

export type NTPStatus = {
	status: number;
	utc_time: string;
	local_time: string;
	server: string;
	uptime: number;
};

export type NTPSettings = {
	enabled: boolean;
	server: string;
	tz_label: string;
	tz_format: string;
};

export type Analytics = {
	max_alloc_heap: number;
	psram_size: number;
	free_psram: number;
	used_psram: number;
	free_heap: number;
	used_heap: number;
	total_heap: number;
	min_free_heap: number;
	core_temp: number;
	fs_total: number;
	fs_used: number;
	uptime: number;
};

export type RSSI = {
	rssi: number;
	ssid: string;
};

export type Battery = {
	soc: number;
	charging: boolean;
};

export type OTAStatus = {
	status: 'none' | 'preparing' | 'progress' | 'finished' | 'error';
	progress: number;
	bytes_written?: number;
	total_bytes?: number;
	error: string;
};

export type StaticSystemInformation = {
	esp_platform: string;
	firmware_version: string;
	build_target: string;
	cpu_freq_mhz: number;
	cpu_type: string;
	cpu_rev: number;
	cpu_cores: number;
	sketch_size: number;
	free_sketch_space: number;
	sdk_version: string;
	arduino_version: string;
	flash_chip_size: number;
	flash_chip_speed: number;
	cpu_reset_reason: string;
};

export type SystemInformation = Analytics & StaticSystemInformation;

export type MQTTStatus = {
	enabled: boolean;
	connected: boolean;
	client_id: string;
	last_error: string;
};

export type MQTTSettings = {
	enabled: boolean;
	uri: string;
	username: string;
	password: string;
	client_id: string;
	keep_alive: number;
	clean_session: boolean;
	message_interval_ms: number;
};

/** An alarm of a device - a view on the alarm entries of its activity log (see $lib/genius/activity). */
export type GeniusAlarm = {
	/** Missing when the gateway's clock was not set at the time. */
	startTime?: Date;
	/** Missing while the alarm lasts. */
	endTime?: Date;
	endingReason: number;
}

export type GeniusSmokeDetectorInfo = {
	sn: number;
	model?: number;
	productionDate?: Date;
	lastSelftest?: Date;
	lastAlarm?: Date;
	deinstallationCount?: number;
	alarmCountTotal?: number;
	alarmCountLast3Months?: number;
	hoursInStorageMode?: number;
	warrantyFlags?: number;
	batteryLowFault?: boolean;
	deviceFault?: boolean;
	driftState?: number;
	dirtForecastNegative?: boolean;
};

export type GeniusRadioModuleInfo = {
	sn: number;
	model?: number;
	lineId?: number;
	lineCharacter?: string;
	lineNumber?: number;
	radioStateMask?: number;
	radioSwitchMask?: number;
	radioInterference?: number;
	radioNetworkFault?: boolean;
	/** True if the alarm line was entered by hand (old FM.Basis / FM.Pro modules). */
	lineManual?: boolean;
	/**
	 * Direct-link signal from the last ConfigCheckProbe range test. Present only when lastRangeTest is set.
	 * `rssi < 0` = reached (measured dBm); `rssi === 0` = tested but no response (out of direct range).
	 */
	rssi?: number;
	/** Timestamp of the last range test this module was included in (unset = never tested). */
	lastRangeTest?: Date;
};

/** One radio module that answered a ConfigCheckProbe survey (heard directly by the gateway). */
export type ConfigCheckResponder = {
	/** Responder's own radio-module serial (packet Org-SN). */
	sn: number;
	/** Best measured RSSI this sweep, in dBm. */
	rssi: number;
	/** Responder's own Line-ID. */
	lineId: number;
	/** Response status/capability byte. */
	status: number;
	/** Group/line nibble byte (high nibble = group A–H, low nibble = line 0–9). */
	groupLine: number;
};

/** Payload of the `config-check-probe` WebSocket event. */
export type ConfigCheckProbeEvent = {
	/** 'started' when a survey begins, 'responder' for each newly-heard module, 'done' when it closes,
	 *  'cancelled' when the user aborts it before completion. */
	phase: 'started' | 'responder' | 'done' | 'cancelled';
	/** Monotonic id correlating the started/responder/done stream. */
	sweepId: number;
	/** Total survey window in ms (present on 'started'). */
	windowMs?: number;
	/** True when the heard module matches a registered device (present on 'responder'). */
	known?: boolean;
	/** The newly-heard module (present on 'responder'). */
	responder?: ConfigCheckResponder;
	/** True when 'done' was reached by an early user stop rather than the survey running to completion. */
	stopped?: boolean;
	/** Distinct responders heard (present on 'done'). */
	responderCount?: number;
	/** Count of known devices whose RSSI was updated (present on 'done'). */
	knownUpdated?: number;
	/** All responders heard this survey (present on 'done'). */
	responders?: ConfigCheckResponder[];
	/** Responders whose radio module is not a registered device (present on 'done'). */
	discovered?: ConfigCheckResponder[];
};

export type GeniusDevice = {
	id: number;
	smokeDetector: GeniusSmokeDetectorInfo;
	radioModule: GeniusRadioModuleInfo;
	location: string;
	registration: number;
	isAlarming: boolean;
	/** Number of alarms of this smoke detector. The alarms themselves are entries of the activity
	 *  log (`events`), which only keeps the newest ones. Backend-owned. */
	alarmCount?: number;
	readoutTime?: Date;
	readoutProtocolVersion?: number;
	/** Entity-ID prefix last announced to Home Assistant. Backend-owned: the gateway
	 *  reports it, but ignores any value a client sends. */
	haEntityIdPrefix?: string;
	/** Current state per status flag (see $lib/genius/status). A flag without an entry is unknown:
	 *  the gateway never heard about it, so it is neither OK nor active. Backend-owned. */
	status?: GeniusFlagState[];
	/** Activity log, oldest first. Backend-owned. */
	events?: GeniusDeviceEvent[];
};

/**
 * Current state of one status flag. Fed by status-change packets received over the air and by
 * acoustic readouts - whichever is newest; a manual reset makes active flags unknown again.
 */
export type GeniusFlagState = {
	/** The flag (a single STATUS_* bit). */
	flag: number;
	/** true = fault / condition present, false = reported OK. */
	active: boolean;
	/** Since when this state holds; missing when the gateway's clock was not set at the time. */
	since?: Date;
	/** Where the state comes from (EVENT_READOUT or EVENT_STATUS_PACKET). */
	source: number;
};

/** One entry of a device's activity log (see EVENT_* in $lib/genius/status). */
export type GeniusDeviceEvent = {
	/** When it happened; missing when the gateway's clock was not set at the time. */
	time?: Date;
	type: number;
	/** Readout / status packet: flags reported active. */
	set?: number;
	/** Readout / status packet: flags reported OK. */
	cleared?: number;
	/** Created: registration type of the device. */
	registration?: number;
	/** Module missing: serial of the radio module reported as missing. */
	cause?: number;
	/** Alarm: when it ended; missing while it lasts. */
	end?: Date;
	/** Alarm: how it ended (GeniusAlarmEnding), AlarmActive while it lasts. */
	endingReason?: number;
};

export type GeniusDevices = {
	version?: number;
	devices: GeniusDevice[];
};

export type AlarmState = {
	isAlarming: boolean;
};

/** Payload of the `device-status` WebSocket event. */
export type DeviceStatusEvent = {
	/** Devices with at least one active fault. */
	numFaulty: number;
	/** Devices reporting a low battery. */
	numBatteryLow: number;
};

export type VisualizerSettings = {
	showDetails: boolean;
	showMetadata: boolean;
};

export type PacketIdentifier = {
	byteNr: number;
	value: number;
	/** If set, only these bits of the byte are compared: (data[byteNr] & mask) === value. */
	mask?: number;
}

export type PacketType = {
	name: string;
	cssClass: string;
	/** On-air message-family byte at offset 26 - groups the message types. */
	familyByte: number;
	/** On-air message-type byte at offset 27 - the type within the family. */
	typeByte: number;
	packetLength: number;
	description: string;
	/** Extra byte matches for sub-variants (e.g. alarm start/stop, line-test start/stop). */
	identifiers: PacketIdentifier[];
};

/** Offset of the message-family byte in a Genius frame (matches backend DATAPOS_MSG_FAMILY). */
export const MSG_FAMILY_POS = 26;
/** Offset of the message-type byte in a Genius frame (matches backend DATAPOS_MSG_TYPE). */
export const MSG_TYPE_POS = 27;

export const PacketTypeNames = {
	Comissioning: 'Commissioning',
	CommissioningProbeRequest: 'CommissioningProbe Request',
	CommissioningProbeResponse: 'CommissioningProbe Response',
	StartLineTest: 'Start Line Test',
	StopLineTest: 'Stop Line Test',
	StartAlarm: 'Start Alarm',
	StopAlarm: 'Stop Alarm',
	StatusChange: 'Status Change',
	ConfigCheckProbeRequest: 'ConfigCheckProbe Request',
	ConfigCheckProbeResponse: 'ConfigCheckProbe Response'
} as const;

/** Names of the packet types that are a status change (alarm start/stop are its alarm-bit cases). */
export const StatusChangePacketTypeNames: string[] = [
	PacketTypeNames.StartAlarm,
	PacketTypeNames.StopAlarm,
	PacketTypeNames.StatusChange
];

// Classification keys on the message-family byte (offset 26) and the message-type byte
// (offset 27) with packetLength as a validator - the same discrimination the radio module
// uses. The type byte repeats across the families: 0x00 is the status change in family 0x00,
// the CommissioningProbe request in family 0x66 and the daily radio link supervision series
// in family 0x33 (same length as the CommissioningProbe request). `identifiers`
// carry sub-variant bytes only (start/stop). First match wins - so a status change setting the
// alarm bit is a Start Alarm, one clearing it a Stop Alarm, and any other a Status Change.
export const PacketTypes: PacketType[] = [
	{
		name: PacketTypeNames.Comissioning,
		cssClass: 'type-comissioning',
		familyByte: 0x66,
		typeByte: 0x03,
		packetLength: 37,
		description: 'Commissioning of new alarm line.',
		identifiers: []
	},
	{
		name: PacketTypeNames.CommissioningProbeRequest,
		cssClass: 'type-commissioningprobe-request',
		familyByte: 0x66,
		typeByte: 0x00,
		packetLength: 28,
		description: 'CommissioningProbe request seen in the commissioning context (not forwarded).',
		identifiers: []
	},
	{
		name: PacketTypeNames.CommissioningProbeResponse,
		cssClass: 'type-commissioningprobe-response',
		familyByte: 0x66,
		typeByte: 0x01,
		packetLength: 32,
		description: 'CommissioningProbe response carrying the requester serial (Req-SN); not forwarded.',
		identifiers: []
	},
	{
		name: PacketTypeNames.StartLineTest,
		cssClass: 'type-linetest-start',
		familyByte: 0x66,
		typeByte: 0x04,
		packetLength: 29,
		description: 'Packets sent to initiate line test function.',
		identifiers: [
			// Line test start: bit 0x04 of the flag byte is set (0x04 and 0x06 were captured) - the
			// same test the gateway firmware uses
			{ byteNr: 28, value: 0x04, mask: 0x04 }
		]
	},
	{
		name: PacketTypeNames.StopLineTest,
		cssClass: 'type-linetest-stop',
		familyByte: 0x66,
		typeByte: 0x04,
		packetLength: 29,
		description: 'Packets sent to end line test function.',
		identifiers: [
			{ byteNr: 28, value: 0x00 }, // Identifier for Line Test Stop
		]
	},
	{
		name: PacketTypeNames.StartAlarm,
		cssClass: 'type-alarm-start',
		familyByte: 0x00,
		typeByte: 0x00,
		packetLength: 36,
		description: 'Status change setting the alarm flag - starts/distributes an alarm.',
		identifiers: [
			{ byteNr: 28, value: 0x01, mask: 0x01 } // Alarm bit in the set mask
		]
	},
	{
		name: PacketTypeNames.StopAlarm,
		cssClass: 'type-alarm-stop',
		familyByte: 0x00,
		typeByte: 0x00,
		packetLength: 36,
		description: 'Status change clearing the alarm flag - stops/silences an alarm.',
		identifiers: [
			{ byteNr: 30, value: 0x01, mask: 0x01 } // Alarm bit in the cleared mask
		]
	},
	{
		name: PacketTypeNames.StatusChange,
		cssClass: 'type-status-change',
		familyByte: 0x00,
		typeByte: 0x00,
		packetLength: 36,
		description:
			'Status change of a smoke detector - flags such as battery low, device fault, radio link fault or dismount set or cleared.',
		identifiers: []
	},
	{
		name: PacketTypeNames.ConfigCheckProbeRequest,
		cssClass: 'type-configcheckprobe-request',
		familyByte: 0x55,
		typeByte: 0x06,
		packetLength: 28,
		description:
			'ConfigCheckProbe request - a silent, direct-range reachability probe (not forwarded). Directly reachable detectors answer with a ConfigCheckProbe Response.',
		identifiers: []
	},
	{
		name: PacketTypeNames.ConfigCheckProbeResponse,
		cssClass: 'type-configcheckprobe-response',
		familyByte: 0x55,
		typeByte: 0x08,
		packetLength: 36,
		description:
			'ConfigCheckProbe response - a directly reachable detector reports its presence, group/line and status (not forwarded).',
		identifiers: []
	}
];

export type GeneralInfo = {
	remainingTxTime: number;
	firstRadioModuleSN: number;
	firstLocation: string;
	secondRadioModuleSN: number;
	secondLocation: string;
	lineID: number;
	lineName: string;
	hops: number;
};

export type CommissioningInfo = {
	newLineID: number;
	timeStr: string;
};

export type CommissioningProbeResponseInfo = {
	requestingRadioModule: number;
	requestingLocation: string;
};

/** Decoded tail of a status-change packet (Start Alarm / Stop Alarm / Status Change). */
export type StatusChangeInfo = {
	/** Status flags newly set (STATUS_* bits, uint16 LE at offset 28). */
	setMask: number;
	/** Status flags newly cleared (uint16 LE at offset 30). */
	clearedMask: number;
	/** Affected smoke detector (uint32 LE at offset 32). */
	smokeDetector: number;
	location: string;
}

export type Packet = {
	id: number;
	timestampFirst: number;
	timestampLast: number;
	type: PacketType | null;
	data: Uint8Array;
	counter: number;
	hash: number;
	generalInfo: GeneralInfo | null;
	specificInfo: CommissioningInfo | CommissioningProbeResponseInfo | StatusChangeInfo | null;
};

export type AlarmLine = {
	id: number;
	name: string;
	created: Date;
	acquisition: number;
};

export type AlarmLines = {
	lines: AlarmLine[];
};

export type CC1101State = {
	state_success: boolean;
	state: number;
}

export type CC1101RadioState = 'unconfigured' | 'initializing' | 'ok' | 'error';

export type CC1101RadioMode = 'idle' | 'rx' | 'tx';

export type CC1101Status = {
	state: CC1101RadioState;
	mode?: CC1101RadioMode;
	configured: boolean;
};

export type CC1101Pins = {
	csn: number;
	miso: number;
	mosi: number;
	sck: number;
	gdo0: number;
	spi_host: number;
	configured?: boolean;
};

export type CC1101Gpio = {
	num: number;
	label: string;
	input: boolean;
	output: boolean;
	reserved: boolean;
	strapping?: boolean;
};

export type CC1101Preset = {
	name: string;
	pins: CC1101Pins;
};

export type CC1101PinProfile = {
	label: string;
	presets: CC1101Preset[];
};

export type CC1101ValidPins = {
	gpios: CC1101Gpio[];
};

export type CC1101ProbeResult = {
	success: boolean;
	spi_ok: boolean;
	chip_detected: boolean;
	gdo0_ok: boolean;
	partnum: number;
	version: number;
	reason?: string;
};

export type WSLoggerSettings = {
	wsLoggerEnabled: boolean;
};

export type HASettings = {
	enabled: boolean;
	discovery_prefix: string;
	device_name: string;
	manufacturer: string;
	model: string;
	rename_entity_ids: boolean;
};

/** Progress of a Home Assistant entity-ID rename. `sent` is the final state: Home
 *  Assistant does not acknowledge the change. `deferred` means there is no MQTT
 *  connection and the rename is caught up on the next connect. */
export type HaEntityRenameState =
	| 'started'
	| 'removing'
	| 'waiting'
	| 'publishing'
	| 'sent'
	| 'deferred'
	| 'failed';

/** Payload of the `ha-entity-rename` WebSocket event. */
export type HaEntityRenameEvent = {
	state: HaEntityRenameState;
	/** Number of smoke detectors renamed in this batch. */
	count: number;
};

/** Latest rename state held on the client; `state` is null until an event arrived. */
export type HaEntityRenameStatus = {
	state: HaEntityRenameState | null;
	count: number;
};

export type ReportSettings = {
	propertyName: string;
	propertyAddress: string;
	customerName: string;
};
