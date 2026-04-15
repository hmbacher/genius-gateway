<script lang="ts">
	import { modals } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { GeniusDevice } from '$lib/types/models';
	import {
		GeniusSmokeDetector,
		GeniusRadioModule,
		GeniusDeviceRegistration
	} from '$lib/types/enums';
	import Cancel from '~icons/tabler/x';
	import Microphone from '~icons/tabler/microphone';
	import MicrophoneOff from '~icons/tabler/microphone-off';
	import CalendarExclamation from '~icons/tabler/calendar-exclamation';
	import Award from '~icons/tabler/award';
	import IconSmokeDetector from '~icons/custom-icons/smoke-detector-m';
	import IconRadioModule from '~icons/custom-icons/radio-module';
	import IconMapPin from '~icons/tabler/map-pin';
	import IconNumber from '~icons/tabler/number';
	import IconFactory from '~icons/tabler/building-factory-2';
	import IconCheckCircle from '~icons/tabler/circle-check';
	import IconCheck from '~icons/tabler/check';
	import IconError from '~icons/tabler/circle-x';
	import IconBattery from '~icons/tabler/battery';
	import IconBatteryOff from '~icons/tabler/battery-off';
	import IconAlarm from '~icons/tabler/bell-ringing';
	import IconShield from '~icons/tabler/shield-check';
	import IconShieldOff from '~icons/tabler/shield-off';
	import IconClock from '~icons/tabler/clock';
	import IconCake from '~icons/tabler/cake';
	import IconSignal from '~icons/tabler/antenna-bars-5';
	import IconHash from '~icons/tabler/hash';
	import IconLetterCase from '~icons/tabler/letter-case';
	import IconForms from '~icons/tabler/forms';
	import IconAccessPoint from '~icons/tabler/access-point';
	import IconShieldX from '~icons/tabler/shield-x';
	import IconRadar from '~icons/tabler/radar-2';
	import IconRefresh from '~icons/tabler/refresh';
	import IconPackages from '~icons/tabler/packages';
	import IconGauge from '~icons/tabler/gauge';
	import IconToggleRightFilled from '~icons/tabler/toggle-right-filled';

	interface Props {
		isOpen: boolean;
		title: string;
		device: GeniusDevice;
		onReadout?: () => void;
	}

	let { isOpen, title, device, onReadout }: Props = $props();

	let isSecureContext = $derived(page.url.protocol === 'https:');

	const smokeDetectorModelName: Record<number, string> = {
		[GeniusSmokeDetector.Unknown]: 'Unknown',
		[GeniusSmokeDetector.GeniusH]: 'Genius H',
		[GeniusSmokeDetector.GeniusHx]: 'Genius Hx',
		[GeniusSmokeDetector.GeniusPlus]: 'Genius Plus',
		[GeniusSmokeDetector.GeniusPlusX]: 'Genius Plus X'
	};

	const radioModuleModelName: Record<number, string> = {
		[GeniusRadioModule.Unknown]: 'Unknown',
		[GeniusRadioModule.None]: 'None',
		[GeniusRadioModule.FmBasis]: 'FM.Basis',
		[GeniusRadioModule.FmPro]: 'FM.Pro',
		[GeniusRadioModule.FmMcp]: 'FM.MCP',
		[GeniusRadioModule.FmBasisX]: 'FM.Basis X',
		[GeniusRadioModule.FmProX]: 'FM.Pro X'
	};

	const registrationName: Record<number, string> = {
		[GeniusDeviceRegistration.BuiltIn]: 'Built-in',
		[GeniusDeviceRegistration.GeniusPacket]: 'Genius Packet',
		[GeniusDeviceRegistration.Manual]: 'Manual',
		[GeniusDeviceRegistration.Acoustic]: 'Acoustic'
	};

	const WARRANTY_FLAG_NAMES: string[] = [
		'Max contamination',
		'Temperature out of range',
		'Detector too old',
		'Storage time exceeded',
		'Activation time exceeded',
		'Too many events',
		'Too many alarms',
		'Too many faults',
		'Too many self-tests',
		'Too many radio faults',
		'Too many radio outages',
		'Radio installation too old',
		'Too much radio activity',
		'Too much radio interference',
		'Too many TX events',
		'Too many RX events'
	];

	const RADIO_STATE_FLAGS: {
		name: string;
		inactive: [string, string, string];
		neutral: boolean;
	}[] = [
		{ name: 'FM Fault', inactive: ['No ', 'FM Fault', ''], neutral: false },
		{ name: 'Range Test active', inactive: ['', 'Range Test', ' is not active'], neutral: true },
		{ name: 'Self-Test active', inactive: ['', 'Self-Test', ' is not active'], neutral: true },
		{ name: 'FM Battery Low', inactive: ['', 'FM Battery', ' is not low'], neutral: false },
		{ name: 'Remote Battery Low', inactive: ['', 'Remote Battery', ' is not low'], neutral: false },
		{ name: 'Remote Error', inactive: ['No ', 'Remote Error', ''], neutral: false },
		{ name: 'Radio Link Error', inactive: ['No ', 'Radio Link Error', ''], neutral: false },
		{ name: 'Remote Alarm', inactive: ['', 'Remote Alarm', ' is not active'], neutral: true }
	];

	const RADIO_SWITCH_CONFIG: [number, string][] = [
		[2, 'Suppress Warnings'],
		[3, 'Suppress Alarms'],
		[4, 'Send Collective Alarm'],
		[5, 'Receive Collective Alarm'],
		[6, 'Radio Link Supervision'],
		[7, 'Reduced TX Power']
	];

	function getAllFlags(mask: number, names: string[]): { name: string; active: boolean }[] {
		return names.map((name, i) => ({ name, active: !!((mask >> i) & 1) }));
	}

	function getSwitchStates(mask: number): { name: string; active: boolean }[] {
		return RADIO_SWITCH_CONFIG.map(([bit, name]) => ({ name, active: !!((mask >> bit) & 1) }));
	}

	function formatDate(date?: Date): string {
		if (!date) return '—';
		return date.toLocaleDateString('de-DE', {
			day: '2-digit',
			month: '2-digit',
			year: 'numeric'
		});
	}

	function formatDateTime(date?: Date): string {
		if (!date) return '—';
		return date.toLocaleString('de-DE', {
			day: '2-digit',
			month: '2-digit',
			year: 'numeric',
			hour: '2-digit',
			minute: '2-digit'
		});
	}

	function formatAge(date?: Date): string {
		if (!date) return '—';
		const now = new Date();
		let years = now.getFullYear() - date.getFullYear();
		const anniversaryThisYear = new Date(date);
		anniversaryThisYear.setFullYear(now.getFullYear());
		if (anniversaryThisYear > now) years--;
		if (years < 1) {
			const totalDays = Math.floor((now.getTime() - date.getTime()) / 86400000);
			return `${totalDays}d`;
		} else {
			const lastAnniversary = new Date(date);
			lastAnniversary.setFullYear(date.getFullYear() + years);
			const remainingDays = Math.floor((now.getTime() - lastAnniversary.getTime()) / 86400000);
			return `${years}y ${remainingDays}d`;
		}
	}

	function handleReadout() {
		if (!isSecureContext) {
			notifications.error('Acoustic device readout requires a secure (HTTPS) connection.', 5000);
			return;
		}
		if (onReadout) {
			modals.close();
			onReadout();
		}
	}
</script>

{#if isOpen}
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center overflow-y-auto"
		transition:fly={{ y: 50 }}
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex w-full max-w-lg flex-col justify-between p-4 shadow-lg"
		>
			<div class="flex items-center justify-between">
				<h2 class="text-base-content text-2xl font-bold">{title}</h2>
				<button class="btn btn-ghost btn-circle btn-sm" onclick={() => modals.close()}>
					<Cancel class="h-5 w-5" />
				</button>
			</div>

			<div class="divider my-2"></div>

			<div class="max-h-[70vh] overflow-y-auto space-y-4 pr-1">
				<!-- General -->
				<div>
					<span class="block bg-base-200 px-2 py-1 rounded-sm text-md font-semibold">General</span>
					<div class="mt-1 space-y-1 text-sm pl-2">
						<div class="flex items-center gap-2">
							<IconMapPin class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Location</span>
							<span class="font-medium">{device.location}</span>
						</div>
						<div class="flex items-center gap-2">
							<IconRadar class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Registration</span>
							{#if device.registration === GeniusDeviceRegistration.Manual}
								<span class="inline-flex items-center gap-1"
									><IconForms class="h-4 w-4" /><span class="font-medium">Manual</span></span
								>
							{:else if device.registration === GeniusDeviceRegistration.GeniusPacket}
								<span class="inline-flex items-center gap-1"
									><IconAccessPoint class="h-4 w-4" /><span class="font-medium">Genius Packet</span
									></span
								>
							{:else if device.registration === GeniusDeviceRegistration.Acoustic}
								<span class="inline-flex items-center gap-1"
									><Microphone class="h-4 w-4" /><span class="font-medium">Acoustic</span></span
								>
							{:else}
								<span class="inline-flex items-center gap-1"
									><Cancel class="h-4 w-4 text-error" /><span
										class="font-medium italic text-base-content/70"
										>{registrationName[device.registration] ?? 'Unknown'}</span
									></span
								>
							{/if}
						</div>
						<div class="flex items-center gap-2">
							<IconClock class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Last Readout</span>
							{#if !device.readoutTime}
								<span class="inline-flex items-center gap-1"
									><MicrophoneOff class="h-4 w-4 text-error" /><span class="font-medium text-error"
										>Never</span
									></span
								>
							{:else if Date.now() - device.readoutTime.getTime() > 365.25 * 24 * 60 * 60 * 1000}
								<span class="inline-flex items-center gap-1"
									><CalendarExclamation class="h-4 w-4 text-error" /><span
										class="font-medium text-error">{formatDateTime(device.readoutTime)}</span
									></span
								>
							{:else}
								<span class="inline-flex items-center gap-1"
									><Award class="h-4 w-4 text-success" /><span class="font-medium"
										>{formatDateTime(device.readoutTime)}</span
									></span
								>
							{/if}
						</div>
					</div>
				</div>

				<!-- Smoke Detector -->
				<div>
					<span class="block bg-base-200 px-2 py-1 rounded-sm text-md font-semibold"
						>Smoke Detector</span
					>
					<div class="mt-1 space-y-1 text-sm pl-2">
						<div class="flex items-center gap-2">
							<IconSmokeDetector class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Model</span>
							<span class="font-medium"
								>{smokeDetectorModelName[device.smokeDetector.model ?? -1] ?? 'Unknown'}</span
							>
						</div>
						<div class="flex items-center gap-2">
							<IconNumber class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Serial Number</span>
							<span class="font-medium">{device.smokeDetector.sn}</span>
						</div>
						<div class="flex items-center gap-2">
							<IconFactory class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Production Date</span>
							<span class="font-medium">{formatDate(device.smokeDetector.productionDate)}</span>
						</div>
						<div class="flex items-center gap-2">
							<IconCake class="h-4 w-4 flex-shrink-0" />
							<span class="text-base-content/60 w-28 flex-shrink-0">Age</span>
							<span class="font-medium">{formatAge(device.smokeDetector.productionDate)}</span>
						</div>

						{#if device.readoutTime}
							<!-- Status attributes from acoustic readout -->
							<div class="divider my-1 mt-3 text-sm text-base-content/40">Status</div>
							<div class="flex items-center gap-2">
								<IconCheckCircle class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Detector Status</span>
								{#if device.smokeDetector.deviceFault}
									<span class="inline-flex items-center gap-1"
										><IconError class="h-4 w-4 flex-shrink-0 text-error" /><span
											class="font-medium text-error">Fault</span
										></span
									>
								{:else}
									<span class="inline-flex items-center gap-1"
										><IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" /><span
											class="font-medium">OK</span
										></span
									>
								{/if}
							</div>
							<div class="flex items-center gap-2 pl-2">
								{#if device.smokeDetector.batteryLowFault}
									<IconBatteryOff class="h-4 w-4 flex-shrink-0" />
								{:else}
									<IconBattery class="h-4 w-4 flex-shrink-0" />
								{/if}
								<span class="text-base-content/60 w-26 flex-shrink-0">Battery</span>
								{#if device.smokeDetector.batteryLowFault}
									<span class="inline-flex items-center gap-1"
										><IconError class="h-4 w-4 flex-shrink-0 text-error" /><span
											class="font-medium text-error">Low</span
										></span
									>
								{:else}
									<span class="inline-flex items-center gap-1"
										><IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" /><span
											class="font-medium">OK</span
										></span
									>
								{/if}
							</div>
							<div class="flex items-center gap-2 pl-2">
								{#if device.smokeDetector.dirtForecastNegative}
									<IconShieldOff class="h-4 w-4 flex-shrink-0" />
								{:else}
									<IconShield class="h-4 w-4 flex-shrink-0" />
								{/if}
								<span class="text-base-content/60 w-26 flex-shrink-0">Dirt Forecast</span>
								{#if device.smokeDetector.dirtForecastNegative}
									<span class="inline-flex items-center gap-1"
										><IconError class="h-4 w-4 flex-shrink-0 text-error" /><span
											class="font-medium text-error">Negative</span
										></span
									>
								{:else}
									<span class="inline-flex items-center gap-1"
										><IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" /><span
											class="font-medium">OK</span
										></span
									>
								{/if}
							</div>
							<div class="flex items-center gap-2 pl-2">
								<IconGauge class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-26 flex-shrink-0">Chamber Drift</span>
								<span class="font-medium">{device.smokeDetector.driftState ?? 0} </span>
							</div>
							<div class="flex items-center gap-2 pl-2">
								{#if (device.smokeDetector.warrantyFlags ?? 0) > 0}
									<IconShieldX class="h-4 w-4 flex-shrink-0" />
								{:else}
									<IconShield class="h-4 w-4 flex-shrink-0" />
								{/if}
								<span class="text-base-content/60 w-26 flex-shrink-0">Warranty</span>
								{#if (device.smokeDetector.warrantyFlags ?? 0) > 0}
									<span class="inline-flex items-center gap-1"
										><IconError class="h-4 w-4 flex-shrink-0 text-error" /><span
											class="font-medium text-error">Voided</span
										></span
									>
								{:else}
									<span class="inline-flex items-center gap-1"
										><IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" /><span
											class="font-medium">OK</span
										></span
									>
								{/if}
							</div>
							{#each getAllFlags(device.smokeDetector.warrantyFlags ?? 0, WARRANTY_FLAG_NAMES) as { name, active }}
								<div class="flex items-center gap-2 pl-8">
									{#if active}
										<IconError class="h-3.5 w-3.5 flex-shrink-0 text-error" />
										<span class="text-xs text-error">{name}</span>
									{:else}
										<IconCheck class="h-3.5 w-3.5 flex-shrink-0 text-success" />
										<span class="text-xs text-base-content/40"><em>{name}</em> not set</span>
									{/if}
								</div>
							{/each}

							<div class="divider my-1 mt-3 text-sm text-base-content/40">Statistics</div>
							<div class="flex items-center gap-2">
								<IconClock class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Last Self-Test</span>
								<span class="font-medium">{formatDate(device.smokeDetector.lastSelftest)}</span>
							</div>
							<div class="flex items-center gap-2">
								<IconAlarm class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Last Alarm</span>
								<span class="font-medium">{formatDate(device.smokeDetector.lastAlarm)}</span>
							</div>
							<div class="flex items-center gap-2">
								<IconAlarm class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Alarms (total)</span>
								<span class="font-medium">{device.smokeDetector.alarmCountTotal ?? 0}</span>
							</div>
							<div class="flex items-center gap-2">
								<IconAlarm class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Alarms (3 months)</span>
								<span class="font-medium">{device.smokeDetector.alarmCountLast3Months ?? 0}</span>
							</div>
							<div class="flex items-center gap-2">
								<IconRefresh class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Deinstallations</span>
								<span class="font-medium">{device.smokeDetector.deinstallationCount ?? 0}</span>
							</div>
							<div class="flex items-center gap-2">
								<IconPackages class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Storage Hours</span>
								<span class="font-medium">{device.smokeDetector.hoursInStorageMode ?? 0}</span>
							</div>
						{/if}
					</div>
				</div>

				<!-- Radio Module -->
				<div>
					<span class="block bg-base-200 px-2 py-1 rounded-sm text-md font-semibold"
						>Radio Module</span
					>
					{#if device.radioModule.model === GeniusRadioModule.None || device.radioModule.model == null}
						<p class="mt-1 text-sm italic text-base-content/60">No radio module installed.</p>
					{:else}
						<div class="mt-1 space-y-1 text-sm pl-2">
							<div class="flex items-center gap-2">
								<IconRadioModule class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Model</span>
								<span class="font-medium"
									>{radioModuleModelName[device.radioModule.model ?? -1] ?? 'Unknown'}</span
								>
							</div>
							<div class="flex items-center gap-2">
								<IconNumber class="h-4 w-4 flex-shrink-0" />
								<span class="text-base-content/60 w-28 flex-shrink-0">Serial Number</span>
								<span class="font-medium">{device.radioModule.sn}</span>
							</div>

							{#if device.readoutTime}
								<div class="divider my-1 mt-3 text-sm text-base-content/40">Radio Status</div>
								<div class="flex items-center gap-2">
									<IconCheckCircle class="h-4 w-4 flex-shrink-0" />
									<span class="text-base-content/60 w-28 flex-shrink-0">Radio Status</span>
									{#if device.radioModule.radioNetworkFault}
										<span class="inline-flex items-center gap-1"
											><IconError class="h-4 w-4 flex-shrink-0 text-error" /><span
												class="font-medium text-error">Fault</span
											></span
										>
									{:else}
										<span class="inline-flex items-center gap-1"
											><IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" /><span
												class="font-medium">OK</span
											></span
										>
									{/if}
								</div>
								{#each RADIO_STATE_FLAGS as flag, i}
									{@const active = !!(((device.radioModule.radioStateMask ?? 0) >> i) & 1)}
									<div class="flex items-center gap-2 pl-6">
										{#if active}
											<IconError class="h-3.5 w-3.5 flex-shrink-0 text-error" />
											<span class="text-xs text-error">{flag.name}</span>
										{:else if flag.neutral}
											<Cancel class="h-3.5 w-3.5 flex-shrink-0 text-base-content/30" />
											<span class="text-xs text-base-content/40"
												>{flag.inactive[0]}<em>{flag.inactive[1]}</em>{flag.inactive[2]}</span
											>
										{:else}
											<IconCheck class="h-3.5 w-3.5 flex-shrink-0 text-success" />
											<span class="text-xs text-base-content/40"
												>{flag.inactive[0]}<em>{flag.inactive[1]}</em>{flag.inactive[2]}</span
											>
										{/if}
									</div>
								{/each}
								{#if (device.radioModule.radioInterference ?? 0) > 0}
									<div class="flex items-center gap-2">
										<IconSignal class="h-4 w-4 flex-shrink-0" />
										<span class="text-base-content/60 w-28 flex-shrink-0">Interference</span>
										<span class="font-medium">{device.radioModule.radioInterference}%</span>
									</div>
								{/if}

								<div class="divider my-1 mt-3 text-sm text-base-content/40">Alarm Line</div>
								{#if device.radioModule.lineId}
									<div class="flex items-center gap-2">
										<IconHash class="h-4 w-4 flex-shrink-0" />
										<span class="text-base-content/60 w-28 flex-shrink-0">Line ID</span>
										<span class="font-medium">{device.radioModule.lineId}</span>
									</div>
								{/if}
								{#if device.radioModule.lineCharacter || device.radioModule.lineNumber != null}
									<div class="flex items-center gap-2">
										<IconLetterCase class="h-4 w-4 flex-shrink-0" />
										<span class="text-base-content/60 w-28 flex-shrink-0">Line</span>
										<span class="font-medium"
											>{device.radioModule.lineCharacter ?? '?'}.{device.radioModule.lineNumber ??
												'?'}</span
										>
									</div>
								{/if}

								<div class="flex items-center gap-2">
									<IconToggleRightFilled class="h-4 w-4 flex-shrink-0" />
									<span class="text-base-content/60 w-28 flex-shrink-0">DIP Switch Config</span>
									<span class="font-medium"></span>
								</div>
								{#each getSwitchStates(device.radioModule.radioSwitchMask ?? 0) as { name, active }}
									<div class="flex items-center gap-2 pl-6">
										{#if active}
											<IconCheck class="h-3.5 w-3.5 flex-shrink-0 text-success" />
											<span class="text-xs"><em>{name}</em> is on</span>
										{:else}
											<Cancel class="h-3.5 w-3.5 flex-shrink-0 text-base-content/30" />
											<span class="text-xs text-base-content/40"><em>{name}</em> is off</span>
										{/if}
									</div>
								{/each}
							{/if}
						</div>
					{/if}
				</div>
			</div>

			<!-- Footer -->
			<div class="divider my-2"></div>
			<div class="flex justify-end gap-2">
				<button class="btn btn-neutral text-neutral-content btn-sm" onclick={() => modals.close()}>
					<Cancel class="h-5 w-5" />
					Close
				</button>
				{#if onReadout}
					<button
						class={isSecureContext
							? 'btn btn-primary text-primary-content btn-sm'
							: 'btn btn-warning text-warning-content btn-sm'}
						onclick={handleReadout}
					>
						<span class="relative inline-flex">
							<Microphone class="h-5 w-5" />
							{#if !isSecureContext}
								<Cancel class="absolute -bottom-1 -right-1.5 h-3.5 w-3.5" />
							{/if}
						</span>
						Update
					</button>
				{/if}
			</div>
		</div>
	</div>
{/if}
