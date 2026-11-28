<script lang="ts">
	import type { Packet, StatusChangeInfo } from '$lib/types/models';
	import { entityStyleSuffix } from '$lib/genius/entity';
	import { STATUS_ALARM, statusFlagLabels } from '$lib/genius/status';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconDetector from '~icons/tabler/alarm-smoke';
	import IconAlarmStart from '~icons/tabler/bell';
	import IconAlarmStop from '~icons/tabler/bell-off';
	import IconStatusSet from '~icons/tabler/alert-triangle';
	import IconStatusCleared from '~icons/tabler/circle-check';

	/*
	 * Status change (36 B, type 0x00): [set mask u16 LE][cleared mask u16 LE][detector serial u32 LE].
	 * Start Alarm / Stop Alarm are the cases where the alarm bit is set / cleared; any other flag
	 * (battery low, faults, ... or one not known yet) is decoded the same way.
	 */
	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();

	let info = $derived(packet.specificInfo as StatusChangeInfo | null);
	let setMask = $derived(info?.setMask ?? 0);
	let clearedMask = $derived(info?.clearedMask ?? 0);

	let setDetails = $derived(
		setMask === 0
			? undefined
			: {
					icon: setMask & STATUS_ALARM ? IconAlarmStart : IconStatusSet,
					text: statusFlagLabels(setMask).join(', '),
					type: setMask & STATUS_ALARM ? 'alarm-start' : 'status-set'
				}
	);

	let clearedDetails = $derived(
		clearedMask === 0
			? undefined
			: {
					icon: clearedMask & STATUS_ALARM ? IconAlarmStop : IconStatusCleared,
					text: statusFlagLabels(clearedMask).join(', '),
					type: clearedMask & STATUS_ALARM ? 'alarm-stop' : 'status-cleared'
				}
	);
</script>

<GeniusPacketContentHeader {packet} {showDetails} />
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(24, 28)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(28, 30)}
	endianess="little"
	details={setDetails}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(30, 32)}
	endianess="little"
	details={clearedDetails}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(32, 36)}
	endianess="little"
	details={{
		icon: IconDetector,
		text: info?.location,
		type: 'serialnumber-smokedetector'.concat(entityStyleSuffix(info?.location))
	}}
/>
