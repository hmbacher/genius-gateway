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

export type GeniusAlarm = {
	startTime: Date;
	endTime: Date;
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
};

export type GeniusDevice = {
	id: number;
	smokeDetector: GeniusSmokeDetectorInfo;
	radioModule: GeniusRadioModuleInfo;
	location: string;
	registration: number;
	isAlarming: boolean;
	alarms: GeniusAlarm[];
	readoutTime?: Date;
	readoutProtocolVersion?: number;
};

export type GeniusDevices = {
	version?: number;
	devices: GeniusDevice[];
};

export type AlarmState = {
	isAlarming: boolean;
};

export type VisualizerSettings = {
	showDetails: boolean;
	showMetadata: boolean;
};

export type PacketIdentifier = {
	byteNr: number;
	value: number;
}

export type PacketType = {
	name: string;
	cssClass: string;
	/** On-air message-type byte at offset 27 — the radio module's primary type discriminator. */
	typeByte: number;
	packetLength: number;
	description: string;
	/** Extra byte matches for sub-variants (e.g. alarm start/stop, line-test start/stop). */
	identifiers: PacketIdentifier[];
};

/** Offset of the message-type byte in a Genius frame (matches backend DATAPOS_MSG_TYPE). */
export const MSG_TYPE_POS = 27;

export const PacketTypeNames = {
	Comissioning: 'Commissioning',
	DiscoveryRequest: 'Discovery Request',
	DiscoveryResponse: 'Discovery Response',
	StartLineTest: 'Start Line Test',
	StopLineTest: 'Stop Line Test',
	StartAlarm: 'Start Alarm',
	StopAlarm: 'Stop Alarm',
	SilentPingRequest: 'SilentPing Request',
	SilentPingResponse: 'SilentPing Response'
} as const;

// Classification keys on the message-type byte (offset 27) with packetLength as a validator —
// the same discrimination the radio module uses. Length alone is ambiguous: typeByte 0x00 is
// shared by Alarming (36 B) and the commissioning Discovery Request (28 B), and a 36-byte frame
// is either Alarming (0x00) or a SilentPing response (0x08). `identifiers` carry sub-variant
// bytes only (start/stop).
export const PacketTypes: PacketType[] = [
	{
		name: PacketTypeNames.Comissioning,
		cssClass: 'type-comissioning',
		typeByte: 0x03,
		packetLength: 37,
		description: 'Commissioning of new alarm line.',
		identifiers: []
	},
	{
		name: PacketTypeNames.DiscoveryRequest,
		cssClass: 'type-discovery-request',
		typeByte: 0x00,
		packetLength: 28,
		description: 'Discovery request seen in the commissioning context (not forwarded).',
		identifiers: []
	},
	{
		name: PacketTypeNames.DiscoveryResponse,
		cssClass: 'type-discovery-response',
		typeByte: 0x01,
		packetLength: 32,
		description: 'Discovery response carrying the requester serial (Req-SN); not forwarded.',
		identifiers: []
	},
	{
		name: PacketTypeNames.StartLineTest,
		cssClass: 'type-linetest-start',
		typeByte: 0x04,
		packetLength: 29,
		description: 'Packets sent to initiate line test function.',
		identifiers: [
			{ byteNr: 28, value: 0x06 }, // Identifier for Line Test Start
		]
	},
	{
		name: PacketTypeNames.StopLineTest,
		cssClass: 'type-linetest-stop',
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
		typeByte: 0x00,
		packetLength: 36,
		description: 'Packet sent to start/distribute an alarm.',
		identifiers: [
			{ byteNr: 28, value: 0x01 }, // Identifier for Alarm Start
		]
	},
	{
		name: PacketTypeNames.StopAlarm,
		cssClass: 'type-alarm-stop',
		typeByte: 0x00,
		packetLength: 36,
		description: 'Packet sent to stop/silence an alarm.',
		identifiers: [
			{ byteNr: 30, value: 0x01 }, // Identifier for Alarm Stop
		]
	},
	{
		name: PacketTypeNames.SilentPingRequest,
		cssClass: 'type-silentping-request',
		typeByte: 0x06,
		packetLength: 28,
		description:
			'Linienabschlusstest / SilentPing request — a silent, direct-range reachability probe (not forwarded). Directly reachable detectors answer with a SilentPing Response.',
		identifiers: []
	},
	{
		name: PacketTypeNames.SilentPingResponse,
		cssClass: 'type-silentping-response',
		typeByte: 0x08,
		packetLength: 36,
		description:
			'Linienabschlusstest / SilentPing response — a directly reachable detector reports its presence, group/line and status (not forwarded).',
		identifiers: []
	}
];

export type GeneralInfo = {
	counter: number;
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

export type DiscoveryResponseInfo = {
	requestingRadioModule: number;
	requestingLocation: string;
};

export type AlarmStartInfo = {
	startingSmokeDetector: number;
	startingLocation: string;
}

export type AlarmStopInfo = {
	silencingSmokeDetector: number;
	silencingLocation: string;
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
	specificInfo: CommissioningInfo | DiscoveryResponseInfo | AlarmStartInfo | AlarmStopInfo | null;
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
};

export type ReportSettings = {
	propertyName: string;
	propertyAddress: string;
	customerName: string;
};
