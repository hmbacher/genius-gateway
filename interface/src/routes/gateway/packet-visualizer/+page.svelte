<script lang="ts">
	import type { PageData } from './$types';
	import { onDestroy, onMount } from 'svelte';
	import { modals } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import type {
		GeniusDevices,
		VisualizerSettings,
		PacketType,
		Packet,
		CommissioningInfo,
		DiscoveryResponseInfo,
		AlarmLines,
		AlarmStartInfo,
		AlarmStopInfo
	} from '$lib/types/models';
	import { PacketTypes, PacketTypeNames } from '$lib/types/models';
	import { jsonDateReviver } from '$lib/utils/misc';
	import { deserializePacket, downloadPacketAsJson } from '$lib/utils/serialization';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import GeniusPacket from '$lib/components/GeniusPacket.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconLogs from '~icons/tabler/logs';
	import IconTrash from '~icons/tabler/trash';
	import IconOK from '~icons/tabler/check';
	import IconCancel from '~icons/tabler/x';
	import IconSettings from '~icons/tabler/adjustments-alt';
	import IconCopy from '~icons/tabler/copy';
	import IconSave from '~icons/tabler/device-floppy';
	import IconLoad from '~icons/tabler/folder-open';

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

	function determinePacketType(data: Uint8Array): PacketType | null {
		for (const type of PacketTypes) {
			if (data.length !== type.packetLength) continue;
			let identifiersMatch = true;
			if (type.identifiers && Array.isArray(type.identifiers)) {
				for (const { byteNr, value } of type.identifiers) {
					if (data[byteNr] !== value) {
						identifiersMatch = false;
						break;
					}
				}
			}
			if (identifiersMatch) {
				return type;
			}
		}
		// No predefined type matches
		return null;
	}

	function interpretPacket(packet: Packet) {
		let dv = new DataView(packet.data.buffer, packet.data.byteOffset, packet.data.byteLength);

		// Common packet properties
		packet.generalInfo = {
			counter: dv.getUint16(1, true),
			firstRadioModuleSN: dv.getUint32(9),
			firstLocation: getDetectorLocationByRMSN(dv.getUint32(9)),
			secondRadioModuleSN: dv.getUint32(14),
			secondLocation: getDetectorLocationByRMSN(dv.getUint32(14)),
			lineID: dv.getUint32(18),
			lineName: getAlarmLineNameByID(dv.getUint32(18)),
			hops: 15 - dv.getUint8(22)
		};

		// Specific packet properties
		if (packet.type?.name === PacketTypeNames.Comissioning) {
			// Alarm Line Comissioning
			packet.specificInfo = {
				newLineID: dv.getUint32(28),
				timeStr: `${dv.getUint8(32).toString().padStart(2, '0')}:${dv.getUint8(33).toString().padStart(2, '0')}:${dv.getUint8(34).toString().padStart(2, '0')}`
			} as CommissioningInfo;
		} else if (packet.type?.name === PacketTypeNames.DiscoveryResponse) {
			// Device Discovery Response
			packet.specificInfo = {
				requestingRadioModule: dv.getUint32(28),
				requestingLocation: getDetectorLocationByRMSN(dv.getUint32(28))
			} as DiscoveryResponseInfo;
		} else if (packet.type?.name === PacketTypeNames.StartAlarm) {
			// Alarm Start
			packet.specificInfo = {
				startingSmokeDetector: dv.getUint32(32),
				startingLocation: getDetectorLocationByRMSN(dv.getUint32(32))
			} as AlarmStartInfo;
		} else if (packet.type?.name === PacketTypeNames.StopAlarm) {
			// Alarm Stop
			packet.specificInfo = {
				silencingSmokeDetector: dv.getUint32(32),
				silencingLocation: getDetectorLocationByRMSN(dv.getUint32(32))
			} as AlarmStopInfo;
		}
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
		const ws_protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
		ws = new WebSocket(`${ws_protocol}://${window.location.host}/ws/logger`);
		ws.binaryType = 'arraybuffer';

		ws.onopen = (ev) => {
			console.log('WebSocket opened. Waiting for packets...');
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
						generalInfo: null,
						specificInfo: null
					});

					// Interpret the packet
					interpretPacket(packets[packets.length - 1]);
				}
			}
		};

		ws.onerror = (ev) => {
			console.log('WebSocket error.', ev);
		};

		ws.onclose = (ev) => {
			ws.close();
			console.log('WebSocket closed.');
		};

		await getGeniusDevices();
		await getAlarmLines();
	});

	onDestroy(async () => {
		ws.close();
		console.log('WebSocket closed.');
	});

	let detectors: GeniusDevices = $state({ devices: [] });

	async function getGeniusDevices() {
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

	let alarmLines: AlarmLines = $state({ lines: [] });

	async function getAlarmLines() {
		try {
			const response = await fetch('/rest/alarm-lines', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			alarmLines = JSON.parse(await response.text(), jsonDateReviver);
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
				notifications.success('Packet visualizer settings updated.', 3000);
				vizSettings = await response.json();
			} else {
				notifications.error('Updating packet visualizer settings failed.', 3000);
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

	async function handleCopyLog() {
		await navigator.clipboard.writeText(JSON.stringify(packets, null, 2));
		notifications.success('Packets log copied to clipboard.', 3000);
	}

	function getDetectorLocationByRMSN(sn: number): string {
		return detectors.devices.find((device) => device.radioModule.sn === sn)?.location || 'Unknown';
	}

	function getAlarmLineNameByID(id: number): string {
		if (id === 0) {
			return 'Not set';
		} else if (id === 0xffffffff) {
			return 'Broadcast';
		} else return alarmLines.lines.find((line) => line.id === id)?.name || 'Unknown';
	}

	let files: any = $state();

	$effect(() => {
		if (files) {
			// Note that `files` is of type `FileList`, not an Array:
			// https://developer.mozilla.org/en-US/docs/Web/API/FileList
			const reader = new FileReader();
			reader.onload = () => {
				const fileContent = reader.result as string;
				try {
					packets = deserializePacket(fileContent) as Packet[];
					if (!packets) {
						notifications.error('Invalid packets log format.', 5000);
						return;
					}

					modals.open(ConfirmDialog, {
						title: 'Re-analyze packets?',
						message: 'Do you want to re-analyze the imported packets?',
						labels: {
							cancel: { label: 'No', icon: IconCancel },
							confirm: { label: 'Yes', icon: IconOK }
						},
						onConfirm: () => {
							packets.forEach((packet) => {
								interpretPacket(packet);
							});
							modals.close();
						}
					});

					notifications.success('Packets log imported.', 3000);
				} catch (error) {
					console.error('Error parsing file:', error);
					notifications.error('Error parsing file.', 5000);
				}
			};

			reader.onerror = (ev) => {
				console.log('Error reading the file:', ev);
				notifications.error('Error reading file.', 5000);
			};

			reader.readAsText(files[0]);
		}
	});

	function handleSavePacketsLogToDisk() {
		const now = new Date();
		const formattedDate = `${String(now.getDate()).padStart(2, '0')}-${String(now.getMonth() + 1).padStart(2, '0')}-${now.getFullYear()}-${String(now.getHours()).padStart(2, '0')}-${String(now.getMinutes()).padStart(2, '0')}-${String(now.getSeconds()).padStart(2, '0')}`;
		downloadPacketAsJson(packets, `genius-packets-${formattedDate}`);
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
		<div>
			<label class="label cursor-pointer w-full justify-between">
				<span class="">Show packet details (data interpretation and highlighting)</span>
				<input
					type="checkbox"
					class="toggle toggle-primary"
					bind:checked={vizSettings.showDetails}
					onchange={() => postVizualizerSettings(vizSettings)}
				/>
			</label>
		</div>
		<div>
			<label class="label cursor-pointer w-full justify-between">
				<span class=""
					>Show meta data of packets (packet number, receive timestamp, repetition, etc.)</span
				>
				<input
					type="checkbox"
					class="toggle toggle-primary"
					bind:checked={vizSettings.showMetadata}
					onchange={() => postVizualizerSettings(vizSettings)}
				/>
			</label>
		</div>
	{/await}
</SettingsCard>

<SettingsCard collapsible={false} maxwidth="max-w-6xl">
	{#snippet icon()}
		<IconLogs class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	{/snippet}
	{#snippet title()}
		<span>Genius Packets</span>
	{/snippet}

	<div class="relative w-full overflow-visible">
		<div class="flex flex-row absolute right-0 -top-13 gap-2 justify-end">
			<div class="tooltip tooltip-left" data-tip="Copy packet data to clipboard">
				<button class="btn btn-primary text-primary-content btn-md" onclick={handleCopyLog}>
					<IconCopy class="h-6 w-6" />
				</button>
			</div>
			<div class="tooltip tooltip-left" data-tip="Clear packets log">
				<button class="btn btn-primary text-primary-content btn-md" onclick={handleClearPacketsLog}>
					<IconTrash class="h-6 w-6" />
				</button>
			</div>
			<div class="divider divider-horizontal my-0 -mx-1"></div>
			<div class="tooltip tooltip-left" data-tip="Load packets log from file">
				<label for="upload" class="btn btn-primary text-primary-content btn-md">
					<IconLoad class="h-6 w-6" />
				</label>
				<input bind:files id="upload" type="file" class="hidden" />
			</div>
			<div class="tooltip tooltip-left" data-tip="Save packets log to file">
				<button
					class="btn btn-primary text-primary-content btn-md"
					disabled={packets.length === 0}
					onclick={handleSavePacketsLogToDisk}
				>
					<IconSave class="h-6 w-6" />
				</button>
			</div>
		</div>

		{#each packets as packet, idx}
			<GeniusPacket
				{packet}
				showMeta={vizSettings.showMetadata}
				showDetails={vizSettings.showDetails}
			/>
		{/each}

		<Spinner text="Waiting for packets..." />
	</div></SettingsCard
>
