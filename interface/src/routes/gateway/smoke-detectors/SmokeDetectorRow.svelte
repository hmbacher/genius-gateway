<script lang="ts">
	import type { GeniusDevice } from '$lib/types/models';
	import {
		hasReadout,
		isStaleReadout,
		getSmokeDetectorFaults,
		getRadioModuleFaults
	} from '$lib/utils/deviceStatus';
	import { formatDate } from '$lib/utils/formatDate';
	import { isOldFmModule } from '$lib/genius/line';
	import { dragHandle } from 'svelte-dnd-action';
	import DetectorStatusBadge from './DetectorStatusBadge.svelte';
	import SignalIndicator from './SignalIndicator.svelte';
	import { GeniusSmokeDetector, GeniusRadioModule } from '$lib/types/enums';
	import type { GeniusRadioModuleInfo } from '$lib/types/models';
	import Delete from '~icons/tabler/trash';
	import Edit from '~icons/tabler/pencil';
	import Logs from '~icons/tabler/logs';
	import ListDetails from '~icons/tabler/list-details';
	import Grip from '~icons/tabler/grip-vertical';
	import Award from '~icons/tabler/award';
	import CalendarExclamation from '~icons/tabler/calendar-exclamation';
	import MicrophoneOff from '~icons/tabler/microphone-off';
	import AntennaOff from '~icons/tabler/antenna-off';
	import LineAlert from '~icons/tabler/alert-triangle';
	import DotsVertical from '~icons/tabler/dots-vertical';
	import Flame from '~icons/tabler/flame-filled';

	interface Props {
		device: GeniusDevice;
		index: number;
		onEdit: (index: number) => void;
		onDelete: (index: number) => void;
		onAlarmLog: (index: number) => void;
		onDetails: (index: number) => void;
	}

	let { device, index, onEdit, onDelete, onAlarmLog, onDetails }: Props = $props();

	function smokeDetectorModelName(model?: number): string {
		switch (model) {
			case GeniusSmokeDetector.GeniusH:
				return 'Genius H';
			case GeniusSmokeDetector.GeniusHx:
				return 'Genius Hx';
			case GeniusSmokeDetector.GeniusPlus:
				return 'Genius Plus';
			case GeniusSmokeDetector.GeniusPlusX:
				return 'Genius Plus X';
			default:
				return '';
		}
	}

	function radioModuleModelName(model?: number): string {
		switch (model) {
			case GeniusRadioModule.FmBasis:
				return 'FM Basis';
			case GeniusRadioModule.FmPro:
				return 'FM Pro';
			case GeniusRadioModule.FmMcp:
				return 'FM MCP';
			case GeniusRadioModule.FmBasisX:
				return 'FM Basis X';
			case GeniusRadioModule.FmProX:
				return 'FM Pro X';
			default:
				return '';
		}
	}

	function hasRadioModule(rm: GeniusRadioModuleInfo): boolean {
		return rm.model !== GeniusRadioModule.None && (rm.sn ?? 0) > 0;
	}

	const sdModelName = $derived(smokeDetectorModelName(device.smokeDetector.model));
	const rmModelName = $derived(radioModuleModelName(device.radioModule.model));
	const rmPresent = $derived(hasRadioModule(device.radioModule));
	const sdReadout = $derived(hasReadout(device));
	const stale = $derived(isStaleReadout(device));
	const sdFaults = $derived(getSmokeDetectorFaults(device.smokeDetector));
	const rmFaults = $derived(getRadioModuleFaults(device.radioModule));
	// Old FM module (FM.Basis / FM.Pro) without a manually-entered alarm line.
	const lineRequired = $derived(
		isOldFmModule(device.radioModule.model) && !device.radioModule.lineCharacter
	);

	// Color helper for top-right service icon (resolves to text-current on
	// alarming rows so the icon stays legible on the red bg).
	const successColor = $derived(device.isAlarming ? 'text-current' : 'text-success');
	const errorColor = $derived(device.isAlarming ? 'text-current' : 'text-error');

	// Drop-focus before navigating away - keeps daisyUI dropdown from staying
	// open after a menu pick. Click-blur is a known DaisyUI quirk.
	function blurAndCall(fn: (i: number) => void) {
		(document.activeElement as HTMLElement)?.blur();
		fn(index);
	}
</script>

<div class="min-w-0">
	<!-- Mobile card (< md) -->
	<div
		class="md:hidden rounded-box {device.isAlarming
			? 'bg-error text-error-content'
			: 'bg-base-100'} p-3 flex flex-col gap-1.5"
	>
		<!-- Row 1: grip · location · service icon · ⋮ menu -->
		<div class="flex items-center gap-2 min-w-0">
			<div class="flex-shrink-0 flex items-center" use:dragHandle>
				<Grip class="h-6 w-6 text-current/30 cursor-grab" />
			</div>
			<div class="flex-1 flex items-center gap-2 min-w-0">
				{#if device.isAlarming}
					<Flame class="flex-shrink-0 h-6 w-6" />
				{/if}
				<div
					class="text-lg font-bold line-clamp-1 flex-1 {device.location === 'Unknown location'
						? 'italic text-current/70'
						: ''}"
				>
					{device.location}
				</div>
				<span
					class="shrink-0 text-base {device.alarms.length > 0
						? 'text-current/70 font-medium'
						: 'text-current/40'}"
				>
					{device.alarms.length}
					{device.alarms.length === 1 ? 'alarm' : 'alarms'}
				</span>
			</div>
			{#if sdReadout && !stale}
				<div class="tooltip tooltip-left" data-tip="Acoustic readout up to date">
					<Award class="flex-shrink-0 h-6 w-6 {successColor}" />
				</div>
			{:else if sdReadout && stale}
				<div class="tooltip tooltip-left" data-tip="Last acoustic readout is more than 1 year ago">
					<CalendarExclamation class="flex-shrink-0 h-6 w-6 {errorColor}" />
				</div>
			{:else}
				<div class="tooltip tooltip-left" data-tip="No acoustic readout performed yet">
					<MicrophoneOff class="flex-shrink-0 h-6 w-6 {errorColor}" />
				</div>
			{/if}
			{#if rmPresent}
				<div class="flex-shrink-0">
					<SignalIndicator
						rssi={device.radioModule.rssi}
						lastRangeTest={device.radioModule.lastRangeTest}
					/>
				</div>
			{/if}
			<div class="dropdown dropdown-end flex-shrink-0">
				<button tabindex="0" class="btn btn-ghost btn-circle btn-sm" aria-label="Open menu">
					<DotsVertical class="h-6 w-6" />
				</button>
				<!-- svelte-ignore a11y_no_noninteractive_tabindex -->
				<ul
					tabindex="0"
					class="dropdown-content menu menu-lg bg-base-100 rounded-box z-10 shadow-lg border border-base-300 p-1 w-44"
				>
					{#if device.alarms.length > 0}
						<li>
							<button onclick={() => blurAndCall(onAlarmLog)}>
								<Logs class="h-6 w-6" /> Alarm log
							</button>
						</li>
					{/if}
					<li>
						<button onclick={() => blurAndCall(onEdit)}>
							<Edit class="h-6 w-6" /> Edit
						</button>
					</li>
					<li>
						<button onclick={() => blurAndCall(onDetails)}>
							<ListDetails class="h-6 w-6" /> Details
						</button>
					</li>
					<li>
						<button class="text-error" onclick={() => blurAndCall(onDelete)}>
							<Delete class="h-6 w-6" /> Delete
						</button>
					</li>
				</ul>
			</div>
		</div>
		<!-- Row 2: smoke detector model · status -->
		<div class="pl-7 flex items-center gap-1.5 flex-wrap">
			{#if sdModelName}
				<span class="font-medium">{sdModelName}</span>
			{:else}
				<span class="italic text-current/50">Unknown model</span>
			{/if}
			<span class="text-current/30">·</span>
			<DetectorStatusBadge
				hasReadout={sdReadout}
				hasFaults={sdFaults.length > 0}
				{stale}
				isAlarming={device.isAlarming}
				faults={sdFaults}
				compact
				onclick={() => onDetails(index)}
			/>
		</div>
		<!-- Row 3: radio module model · status -->
		<div class="pl-7 flex items-center gap-1.5 flex-wrap">
			{#if !rmPresent}
				<AntennaOff class="h-6 w-6 text-current/40" />
				<span class="italic text-current/40">No radio module</span>
			{:else}
				{#if rmModelName}
					<span class="font-medium">{rmModelName}</span>
				{:else}
					<span class="italic text-current/50">Unknown model</span>
				{/if}
				<span class="text-current/30">·</span>
				<DetectorStatusBadge
					hasReadout={sdReadout}
					hasFaults={rmFaults.length > 0}
					{stale}
					isAlarming={device.isAlarming}
					faults={rmFaults}
					compact
					onclick={() => onDetails(index)}
				/>
				{#if lineRequired}
					<button
						class="btn btn-warning btn-xs gap-1"
						title="Set the alarm line"
						onclick={() => onEdit(index)}
					>
						<LineAlert class="h-3.5 w-3.5" /> Line required
					</button>
				{/if}
			{/if}
		</div>
	</div>

	<!-- Desktop row (≥ md) -->
	<div
		class="hidden md:grid rounded-box {device.isAlarming
			? 'bg-error text-error-content'
			: 'bg-base-100'} grid-cols-[30px_1fr_1fr_1fr_65px_72px_120px] gap-2 px-4 py-2 items-center"
	>
		<!-- Drag handle -->
		<div class="flex items-center justify-center" use:dragHandle>
			<Grip class="h-6 w-6 text-current/30 cursor-grab" />
		</div>

		<!-- Location -->
		<div class="flex items-center gap-1.5 min-w-0">
			{#if device.isAlarming}
				<Flame class="flex-shrink-0 h-5 w-5" />
			{/if}
			<div
				class="text-sm font-bold {device.location === 'Unknown location'
					? 'italic text-current/70'
					: ''} truncate"
			>
				{device.location}
			</div>
		</div>

		<!-- Smoke Detector -->
		<div class="text-sm min-w-0">
			<div class="truncate">
				{#if sdModelName}
					<span class="font-medium">{sdModelName}</span>
				{:else}
					<span class="italic text-current/50">Unknown model</span>
				{/if}
			</div>
			<DetectorStatusBadge
				hasReadout={sdReadout}
				hasFaults={sdFaults.length > 0}
				{stale}
				isAlarming={device.isAlarming}
				faults={sdFaults}
				onclick={() => onDetails(index)}
			/>
		</div>

		<!-- Radio Module -->
		<div class="text-sm min-w-0">
			{#if !rmPresent}
				<div class="flex items-center text-current/40 italic">
					<AntennaOff class="flex-shrink-0 mr-1 h-4 w-4" />
					<span class="truncate">No radio module</span>
				</div>
			{:else}
				<div class="truncate">
					{#if rmModelName}
						<span class="font-medium">{rmModelName}</span>
					{:else}
						<span class="italic text-current/50">Unknown model</span>
					{/if}
				</div>
				<DetectorStatusBadge
					hasReadout={sdReadout}
					hasFaults={rmFaults.length > 0}
					{stale}
					isAlarming={device.isAlarming}
					faults={rmFaults}
					onclick={() => onDetails(index)}
				/>
				{#if lineRequired}
					<button
						class="btn btn-warning btn-xs gap-1"
						title="Set the alarm line"
						onclick={() => onEdit(index)}
					>
						<LineAlert class="h-3.5 w-3.5" /> Line required
					</button>
				{/if}
			{/if}
		</div>

		<!-- Alarms -->
		<div class="text-center text-sm">
			<div>{device.alarms.length}</div>
			{#if device.alarms.length > 0}
				<div>{formatDate(device.alarms[device.alarms.length - 1].startTime)}</div>
			{/if}
		</div>

		<!-- Service: readout age + direct signal -->
		<div class="flex items-center justify-center gap-1.5">
			{#if sdReadout && !stale}
				<div class="tooltip tooltip-left" data-tip="Acoustic readout up to date">
					<Award class="h-6 w-6 {successColor}" />
				</div>
			{:else if sdReadout && stale}
				<div class="tooltip tooltip-left" data-tip="Last acoustic readout is more than 1 year ago">
					<CalendarExclamation class="h-6 w-6 {errorColor}" />
				</div>
			{:else}
				<div class="tooltip tooltip-left" data-tip="No acoustic readout performed yet">
					<MicrophoneOff class="h-6 w-6 {errorColor}" />
				</div>
			{/if}
			{#if rmPresent}
				<SignalIndicator
					rssi={device.radioModule.rssi}
					lastRangeTest={device.radioModule.lastRangeTest}
				/>
			{/if}
		</div>

		<!-- Manage buttons -->
		<div class="flex items-center justify-end">
			<span class="inline-flex flex-row">
				{#if device.alarms.length > 0}
					<div class="tooltip tooltip-left" data-tip="Show alarm log">
						<button
							class="btn btn-ghost btn-circle btn-sm"
							aria-label="Show alarm log"
							onclick={() => onAlarmLog(index)}
						>
							<Logs class="h-6 w-6" />
						</button>
					</div>
				{/if}
				<div class="tooltip tooltip-left" data-tip="Edit smoke detector">
					<button
						class="btn btn-ghost btn-circle btn-sm"
						aria-label="Edit smoke detector"
						onclick={() => onEdit(index)}
					>
						<Edit class="h-6 w-6" />
					</button>
				</div>
				<div class="tooltip tooltip-left" data-tip="Device details">
					<button
						class="btn btn-ghost btn-circle btn-sm"
						aria-label="Device details"
						onclick={() => onDetails(index)}
					>
						<ListDetails class="h-6 w-6" />
					</button>
				</div>
				<div class="tooltip tooltip-left" data-tip="Delete smoke detector">
					<button
						class="btn btn-ghost btn-circle btn-sm"
						aria-label="Delete smoke detector"
						onclick={() => onDelete(index)}
					>
						<Delete class="h-6 w-6 {errorColor}" />
					</button>
				</div>
			</span>
		</div>
	</div>
</div>
