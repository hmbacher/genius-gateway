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
	free_heap: number;
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

export type DownloadOTA = {
	status: string;
	progress: number;
	error: string;
};

export type StaticSystemInformation = {
	esp_platform: string;
	firmware_version: string;
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
};

export type GeniusAlarm = {
	startTime: Date;
	endTime: Date;
	endingReason: number;
}

export type GeniusComponent = {
	sn: number;
	model?: number;
	productionDate?: Date;
};

export type GeniusDevice = {
	smokeDetector: GeniusComponent;
	radioModule: GeniusComponent;
	location: string;
	registration: number;
	isAlarming: boolean;
	alarms: GeniusAlarm[];
};

export type GeniusDevices = {
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
	packetLength: number;
	description: string;
	identifiers: PacketIdentifier[];
};

export const PacketTypeNames = {
	Comissioning: 'Commissioning',
	DiscoveryRequest: 'Discovery Request',
	DiscoveryResponse: 'Discovery Response',
	StartLineTest: 'Start Line Test',
	StopLineTest: 'Stop Line Test',
	StartAlarm: 'Start Alarm',
	StopAlarm: 'Stop Alarm'
} as const;

export const PacketTypes: PacketType[] = [
	{
		name: PacketTypeNames.Comissioning,
		cssClass: 'type-comissioning',
		packetLength: 37,
		description: 'Commissioning of new alarm line.',
		identifiers: []
	},
	{
		name: PacketTypeNames.DiscoveryRequest,
		cssClass: 'type-discovery-request',
		packetLength: 28,
		description: 'Purpose unknown, possibly a device discovery request.',
		identifiers: []
	},
	{
		name: PacketTypeNames.DiscoveryResponse,
		cssClass: 'type-discovery-response',
		packetLength: 32,
		description: 'Purpose unknown, possibly a device discovery response.',
		identifiers: []
	},
	{
		name: PacketTypeNames.StartLineTest,
		cssClass: 'type-linetest-start',
		packetLength: 29,
		description: 'Packets sent to initiate line test function.',
		identifiers: [
			{ byteNr: 28, value: 0x06 }, // Identifier for Line Test Start
		]
	},
	{
		name: PacketTypeNames.StopLineTest,
		cssClass: 'type-linetest-stop',
		packetLength: 29,
		description: 'Packets sent to end line test function.',
		identifiers: [
			{ byteNr: 28, value: 0x00 }, // Identifier for Line Test Stop
		]
	},
	{
		name: PacketTypeNames.StartAlarm,
		cssClass: 'type-alarm-start',
		packetLength: 36,
		description: 'Packet sent to start/distribute an alarm.',
		identifiers: [
			{ byteNr: 28, value: 0x01 }, // Identifier for Alarm Start
		]
	},
	{
		name: PacketTypeNames.StopAlarm	,
		cssClass: 'type-alarm-stop',
		packetLength: 36,
		description: 'Packet sent to stop/silence an alarm.',
		identifiers: [
			{ byteNr: 30, value: 0x01 }, // Identifier for Alarm Stop
		]
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