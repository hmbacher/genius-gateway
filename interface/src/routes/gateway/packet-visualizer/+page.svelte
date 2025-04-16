<script lang="ts">
	import type { PageData } from './$types';
	import { onDestroy, onMount } from 'svelte';
	import { modals } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { HekatronDevices, VisualizerSettings, PacketType, CommissioningPacket, Packet } from '$lib/types/models';
	import { PacketTypes } from '$lib/types/models';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import GeniusPacket from '$lib/components/GeniusPacket.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconLogs from '~icons/tabler/logs';
	import IconTrash from '~icons/tabler/trash';
	import IconCancel from '~icons/tabler/x';
	import IconSettings from '~icons/tabler/adjustments-alt';
	import IconCopy from '~icons/tabler/copy';

	const OFFSET_TIMESTAMP = 0; // Offset for the timestamp in the byte array
	const OFFSET_DATA_LENGTH = 76; // Offset for the data length in the byte array
	const OFFSET_DATA_START = 9; // Offset where the actual data starts in the byte array

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	let vizSettings: VisualizerSettings = $state({
		showDetails: true,
		showMetadata: true
	});

	let packets: Packet[] = $state([]);

	// packets = [
	// 	{
	// 		id: 1,
	// 		timestampFirst: 2475436727,
	// 		timestampLast: 2475536727,
	// 		type: PacketTypes.Commissioning,
	// 		counter: 308,
	// 		hash: 0,
	// 		data: new Uint8Array([
	// 			2, 59, 24, 0, 255, 255, 255, 255, 0, 131, 5, 81, 60, 0, 131, 5, 81, 60, 204, 144, 89, 60,
	// 			15, 198, 69, 0, 102, 3, 204, 144, 89, 60, 10, 3, 17, 89, 0
	// 		]),
	// 		interpreted: {
	// 			counter: 6203,
	// 			firstRadioModuleSN: 2198163772,
	// 			firstLocation: 'Wohnzimmer',
	// 			secondRadioModuleSN: 2198163772,
	// 			secondLocation: 'Unknown',
	// 			hops: 0,
	// 			currentLineID: 3432012092,
	// 			newLineID: 3432012092,
	// 			timeStr: '10:03:17',
	// 		}
	// 	},
	// 	{
	// 		id: 2,
	// 		timestampFirst: 2475436727,
	// 		timestampLast: 2475536727,
	// 		type: PacketTypes.Unknown,
	// 		counter: 10,
	// 		hash: 0,
	// 		data: new Uint8Array([
	// 			2, 59, 24, 0, 255, 255, 255, 255, 0, 131, 5, 81, 60, 0, 131, 5, 81, 60, 204, 144, 89, 60,
	// 			15, 198, 69, 0, 102, 3, 204, 144, 89, 60
	// 		]),
	// 		interpreted: null
	// 	},
	// 	{
	// 		id: 3,
	// 		timestampFirst: 2475436727,
	// 		timestampLast: 2475536727,
	// 		type: PacketTypes.Unknown,
	// 		counter: 10,
	// 		hash: 0,
	// 		data: new Uint8Array([
	// 			2, 59, 24, 0, 255, 255, 255, 255, 0, 131, 5, 81, 69, 0, 131, 5, 81, 60, 204, 144, 89, 60,
	// 			15, 198, 69, 0, 102, 3, 204, 144
	// 		]),
	// 		interpreted: null
	// 	}
	// ];

	function determinePacketType(data: Uint8Array): PacketType {
		for (const key in PacketTypes) {
			const packetType = PacketTypes[key as keyof typeof PacketTypes];
			if (data.length === packetType.packetLength) {
				return packetType;
			}
		}
		return PacketTypes.Unknown;
	}	

	function interpretPacket(packet: Packet) {
		let dv = new DataView(packet.data.buffer);
		if (packet.type.name === PacketTypes.Commissioning.name) {
			// Alarm Line Cmissioning
			return {
				counter: dv.getUint16(1, true),
				firstRadioModuleSN: dv.getUint32(9),
				firstLocation: getDetectorLocationByRMSN(dv.getUint32(9)),
				secondRadioModuleSN: dv.getUint32(14),
				secondLocation: getDetectorLocationByRMSN(dv.getUint32(14)),
				currentLineID: dv.getUint32(18),
				hops: 15 - dv.getUint8(22),
				newLineID: dv.getUint32(23),
				timeStr: `${dv.getUint8(32)}:${dv.getUint8(33)}:${dv.getUint8(34)}`
			};
		}
		else if (packet.type.name === PacketTypes.DiscoveryRequest.name) {
			// Device Discovery Request (?)
			return {
				counter: dv.getUint16(1, true),
				firstRadioModuleSN: dv.getUint32(9),
				firstLocation: getDetectorLocationByRMSN(dv.getUint32(9)),
				secondRadioModuleSN: dv.getUint32(14),
				secondLocation: getDetectorLocationByRMSN(dv.getUint32(14)),
				lineID: dv.getUint32(18),
				hops: 15 - dv.getUint8(22)
			};
		}
		else if (packet.type.name === PacketTypes.DiscoveryResponse.name) {
			// Device Discovery Response (?)
			return {
				counter: dv.getUint16(1, true),
				firstRadioModuleSN: dv.getUint32(9),
				firstLocation: getDetectorLocationByRMSN(dv.getUint32(9)),
				secondRadioModuleSN: dv.getUint32(14),
				secondLocation: getDetectorLocationByRMSN(dv.getUint32(14)),
				lineID: dv.getUint32(18),
				hops: 15 - dv.getUint8(22),
				requestingRadioModule: dv.getUint32(23),
				requestingLocation: getDetectorLocationByRMSN(dv.getUint32(32))
			};
		}

		return null;
	}

	function calculateHash(data: Uint8Array): number {
		let hash = 2166136261; // FNV-1a 32-bit offset basis
		for (let i = 3; i < data.length; i++) {
			// Skip first 3 bytes (0x02 and two counter bytes)
			hash ^= data[i];
			hash = (hash * 16777619) >>> 0; // FNV-1a 32-bit prime
		}
		return hash;
	}
	
	let ws: WebSocket;

	onMount(async () => {
		ws = new WebSocket(`ws://${window.location.host}/ws/logger`);
		ws.binaryType = 'arraybuffer';

		ws.onopen = (ev) => {
			console.log('WebSocket opened');
		};

		ws.onmessage = (ev) => {
			if (ev.data instanceof ArrayBuffer) {
				let dv = new DataView(ev.data);
				let datalength = dv.getUint32(OFFSET_DATA_LENGTH, true);
				let timestamp = dv.getUint32(OFFSET_TIMESTAMP, true);
				let databuf = new Uint8Array(ev.data, OFFSET_DATA_START, datalength);

				let lastpacket = packets.length > 0 ? packets[packets.length - 1] : null;
				let hash = calculateHash(databuf);

				if (lastpacket && lastpacket.hash === hash) {
					// Packet with this hash already exists, so just update the last packet
					lastpacket.counter++;
					lastpacket.timestampLast = dv.getUint32(OFFSET_TIMESTAMP, true);
				} else {
					// New packet, so create a new entry
					packets.push({
						id: packets.length + 1,
						timestampFirst: timestamp,
						timestampLast: timestamp,
						type: determinePacketType(databuf),
						data: databuf,
						counter: 1,
						hash: hash,
						interpreted: null
					});

					// Interpret the packet
					packets[packets.length - 1].interpreted = interpretPacket(packets[packets.length - 1]);

				}
			}
		};

		ws.onerror = (ev) => {
			console.log('WebSocket error', ev);
		};

		ws.onclose = (ev) => {
			ws.close();
			console.log('WebSocket closed');
		};

		await getHekatronDevices();
	});

	onDestroy(async () => {
		ws.close();
		console.log('WebSocket closed');
	});

	let detectors: HekatronDevices = $state({ devices: [] });

	async function getHekatronDevices() {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			detectors = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function getVizualizerSettings() {
		try {
			const response = await fetch('/rest/packet-visualizer', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			vizSettings = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postVizualizerSettings(settings: VisualizerSettings) {
		try {
			const response = await fetch('/rest/packet-visualizer', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(settings)
			});

			if (response.status == 200) {
				notifications.success('Packet vizualizer settings updated.', 3000);
				vizSettings = await response.json();
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	function handleClearPacketsLog() {
		modals.open(ConfirmDialog, {
			title: 'Confirm',
			message: 'Are you sure you want to clear the packet logs?',
			labels: {
				cancel: { label: 'Abort', icon: IconCancel },
				confirm: { label: 'Clear', icon: IconTrash }
			},
			onConfirm: () => {
				packets = [];
				modals.close();
			}
		});
	}

	function handleCopyLog() {
		notifications.success('Packets log copied to clipboard.', 3000);
	}

	let toHex = (num: number) => {
		return num.toString(16).padStart(2, '0').toUpperCase();
	};

	function getDetectorLocationByRMSN(sn: number): string {
		return detectors.devices.find((device) => device.radioModule.sn === sn)?.location || 'Unknown';
	}
</script>

<SettingsCard collapsible={true}>
	{#snippet icon()}
		<IconSettings class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	{/snippet}
	{#snippet title()}
		<span>Vizualizer Settings</span>
	{/snippet}
	{#await getVizualizerSettings()}
		<Spinner text="Loading settings..." />
	{:then nothing}
		<div class="relative w-full overflow-visible">
			<div class="form-control">
				<label class="label cursor-pointer">
					<span class="">Show packet details (data interpretation and highlighting)</span>
					<input
						type="checkbox"
						class="toggle toggle-primary"
						bind:checked={vizSettings.showDetails}
						onchange={() => postVizualizerSettings(vizSettings)}
					/>
				</label>
				<label class="label cursor-pointer">
					<span class="">Show meta data of packets (packet number, receive timestamp, repetition, etc.)</span>
					<input
						type="checkbox"
						class="toggle toggle-primary"
						bind:checked={vizSettings.showMetadata}
						onchange={() => postVizualizerSettings(vizSettings)}
					/>
				</label>
			</div>
		</div>
	{/await}
</SettingsCard>

<SettingsCard collapsible={false} maxwidth="max-w-6xl">
	{#snippet icon()}
		<IconLogs class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	{/snippet}
	{#snippet title()}
		<span>Hekatron Genius Packets</span>
	{/snippet}

	<div class="relative w-full overflow-visible">
		<button
			class="btn btn-primary text-primary-content btn-md absolute -top-14 right-16"
			onclick={handleCopyLog}
		>
			<IconCopy class="h-6 w-6" />
		</button>
		<button
			class="btn btn-primary text-primary-content btn-md absolute -top-14 right-0"
			onclick={handleClearPacketsLog}
		>
			<IconTrash class="h-6 w-6" />
		</button>

		{#each packets as packet, idx}
			<GeniusPacket {packet} showMeta={vizSettings.showMetadata} showDetails={vizSettings.showDetails} />
		{/each}

		<Spinner text="Waiting for packets..." />
	</div>
</SettingsCard>
