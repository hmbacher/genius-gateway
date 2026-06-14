<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { GeniusDevice } from '$lib/types/models';
	import {
		GeniusSmokeDetector,
		GeniusRadioModule,
		GeniusDeviceRegistration
	} from '$lib/types/enums';
	import { isStaleReadout } from '$lib/utils/deviceStatus';
	import { formatDate, formatDateTime, formatAge } from '$lib/utils/formatDate';
	import DetailRow from './DetailRow.svelte';
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
	import IconWarning from '~icons/tabler/alert-triangle';
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

	interface Props extends ModalProps {
		title: string;
		device: GeniusDevice;
		onReadout?: () => void;
	}

	let { isOpen, title, device, onReadout }: Props = $props();

	const titleId = `device-details-title-${Math.random().toString(36).slice(2)}`;

	let isSecureContext = $derived(page.url.protocol === 'https:');
	let staleReadout = $derived(isStaleReadout(device));

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
		warn?: boolean;
	}[] = [
		{ name: 'FM Fault', inactive: ['No ', 'FM Fault', ''], neutral: false },
		{ name: 'Range Test active', inactive: ['', 'Range Test', ' is not active'], neutral: true },
		{ name: 'Self-Test active', inactive: ['', 'Self-Test', ' is not active'], neutral: true },
		{ name: 'FM Battery Low', inactive: ['', 'FM Battery', ' is not low'], neutral: false },
		{ name: 'Remote Battery Low', inactive: ['', 'Remote Battery', ' is not low'], neutral: false, warn: true },
		{ name: 'Remote Error', inactive: ['No ', 'Remote Error', ''], neutral: false, warn: true },
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

	let warrantyFlags = $derived(
		WARRANTY_FLAG_NAMES.map((name, i) => ({
			name,
			active: !!(((device.smokeDetector.warrantyFlags ?? 0) >> i) & 1)
		}))
	);

	let switchStates = $derived(
		RADIO_SWITCH_CONFIG.map(([bit, name]) => ({
			name,
			active: !!(((device.radioModule.radioSwitchMask ?? 0) >> bit) & 1)
		}))
	);

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
		aria-modal="true"
		aria-labelledby={titleId}
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center overflow-y-auto"
		transition:fly={{ y: 50 }}
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex w-full max-w-lg flex-col justify-between p-4 shadow-lg"
		>
			<div class="flex items-center justify-between">
				<h2 id={titleId} class="text-base-content text-2xl font-bold">{title}</h2>
				<button
					class="btn btn-ghost btn-circle btn-sm"
					aria-label="Close"
					onclick={() => modals.close()}
				>
					<Cancel class="h-5 w-5" />
				</button>
			</div>

			<div class="divider my-2"></div>

			<div class="max-h-[70vh] overflow-y-auto space-y-4 pr-1">
				<!-- General -->
				<div>
					<span class="block bg-base-200 px-2 py-1 rounded-sm text-base font-semibold">General</span
					>
					<div class="mt-1 space-y-1 text-sm pl-2">
						<DetailRow icon={IconMapPin} label="Location" value={device.location} />

						<DetailRow icon={IconRadar} label="Registration">
							{#if device.registration === GeniusDeviceRegistration.Manual}
								<span class="inline-flex items-center gap-1">
									<IconForms class="h-4 w-4" /><span class="font-medium">Manual</span>
								</span>
							{:else if device.registration === GeniusDeviceRegistration.GeniusPacket}
								<span class="inline-flex items-center gap-1">
									<IconAccessPoint class="h-4 w-4" /><span class="font-medium">Genius Packet</span>
								</span>
							{:else if device.registration === GeniusDeviceRegistration.Acoustic}
								<span class="inline-flex items-center gap-1">
									<Microphone class="h-4 w-4" /><span class="font-medium">Acoustic</span>
								</span>
							{:else}
								<span class="inline-flex items-center gap-1">
									<Cancel class="h-4 w-4 text-error" />
									<span class="font-medium italic text-base-content/70"
										>{registrationName[device.registration] ?? 'Unknown'}</span
									>
								</span>
							{/if}
						</DetailRow>

						<DetailRow icon={IconClock} label="Last Readout">
							{#if !device.readoutTime}
								<span class="inline-flex items-center gap-1">
									<MicrophoneOff class="h-4 w-4 text-error" />
									<span class="font-medium text-error">Never</span>
								</span>
							{:else if staleReadout}
								<span class="inline-flex items-center gap-1">
									<CalendarExclamation class="h-4 w-4 text-error" />
									<span class="font-medium text-error">{formatDateTime(device.readoutTime)}</span>
									<span class="text-base-content/50">({formatAge(device.readoutTime)} ago)</span>
								</span>
							{:else}
								<span class="inline-flex items-center gap-1">
									<Award class="h-4 w-4 text-success" />
									<span class="font-medium">{formatDateTime(device.readoutTime)}</span>
									<span class="text-base-content/50">({formatAge(device.readoutTime)} ago)</span>
								</span>
							{/if}
						</DetailRow>
					</div>
				</div>

				<!-- Smoke Detector -->
				<div>
					<span class="block bg-base-200 px-2 py-1 rounded-sm text-base font-semibold"
						>Smoke Detector</span
					>
					<div class="mt-1 space-y-1 text-sm pl-2">
						<DetailRow
							icon={IconSmokeDetector}
							label="Model"
							value={smokeDetectorModelName[device.smokeDetector.model ?? -1] ?? 'Unknown'}
						/>
						<DetailRow icon={IconNumber} label="Serial Number" value={device.smokeDetector.sn} />
						<DetailRow
							icon={IconFactory}
							label="Production Date"
							value={formatDate(device.smokeDetector.productionDate)}
						/>
						<DetailRow
							icon={IconCake}
							label="Age"
							value={formatAge(device.smokeDetector.productionDate)}
						/>

						{#if device.readoutTime}
							<!-- Status attributes from acoustic readout -->
							<div class="divider my-1 mt-3 text-sm text-base-content/40">Status</div>

							<DetailRow icon={IconCheckCircle} label="Detector Status">
								{#if device.smokeDetector.deviceFault}
									<span class="inline-flex items-center gap-1">
										<IconError class="h-4 w-4 flex-shrink-0 text-error" />
										<span class="font-medium text-error">Fault</span>
									</span>
								{:else}
									<span class="inline-flex items-center gap-1">
										<IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" />
										<span class="font-medium">OK</span>
									</span>
								{/if}
							</DetailRow>

							<DetailRow
								icon={device.smokeDetector.batteryLowFault ? IconBatteryOff : IconBattery}
								label="Battery"
								indent="sm"
							>
								{#if device.smokeDetector.batteryLowFault}
									<span class="inline-flex items-center gap-1">
										<IconError class="h-4 w-4 flex-shrink-0 text-error" />
										<span class="font-medium text-error">Low</span>
									</span>
								{:else}
									<span class="inline-flex items-center gap-1">
										<IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" />
										<span class="font-medium">OK</span>
									</span>
								{/if}
							</DetailRow>

							<DetailRow
								icon={device.smokeDetector.dirtForecastNegative ? IconShieldOff : IconShield}
								label="Dirt Forecast"
								indent="sm"
							>
								{#if device.smokeDetector.dirtForecastNegative}
									<span class="inline-flex items-center gap-1">
										<IconError class="h-4 w-4 flex-shrink-0 text-error" />
										<span class="font-medium text-error">Negative</span>
									</span>
								{:else}
									<span class="inline-flex items-center gap-1">
										<IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" />
										<span class="font-medium">OK</span>
									</span>
								{/if}
							</DetailRow>

							<DetailRow
								icon={IconGauge}
								label="Chamber Drift"
								value={device.smokeDetector.driftState ?? 0}
								indent="sm"
							/>

							<DetailRow
								icon={(device.smokeDetector.warrantyFlags ?? 0) > 0 ? IconShieldX : IconShield}
								label="Warranty"
								indent="sm"
							>
								{#if (device.smokeDetector.warrantyFlags ?? 0) > 0}
									<span class="inline-flex items-center gap-1">
										<IconError class="h-4 w-4 flex-shrink-0 text-error" />
										<span class="font-medium text-error">Voided</span>
									</span>
								{:else}
									<span class="inline-flex items-center gap-1">
										<IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" />
										<span class="font-medium">OK</span>
									</span>
								{/if}
							</DetailRow>

							{#each warrantyFlags as { name, active }}
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

							<DetailRow
								icon={IconClock}
								label="Last Self-Test"
								value={formatDate(device.smokeDetector.lastSelftest)}
							/>
							<DetailRow
								icon={IconAlarm}
								label="Last Alarm"
								value={formatDate(device.smokeDetector.lastAlarm)}
							/>
							<DetailRow
								icon={IconAlarm}
								label="Alarms (total)"
								value={device.smokeDetector.alarmCountTotal ?? 0}
							/>
							<DetailRow
								icon={IconAlarm}
								label="Alarms (3 months)"
								value={device.smokeDetector.alarmCountLast3Months ?? 0}
							/>
							<DetailRow
								icon={IconRefresh}
								label="Deinstallations"
								value={device.smokeDetector.deinstallationCount ?? 0}
							/>
							<DetailRow
								icon={IconPackages}
								label="Storage Hours"
								value={device.smokeDetector.hoursInStorageMode ?? 0}
							/>
						{/if}
					</div>
				</div>

				<!-- Radio Module -->
				<div>
					<span class="block bg-base-200 px-2 py-1 rounded-sm text-base font-semibold"
						>Radio Module</span
					>
					{#if device.radioModule.model === GeniusRadioModule.None || device.radioModule.model == null}
						<p class="mt-1 text-sm italic text-base-content/60">No radio module installed.</p>
					{:else}
						<div class="mt-1 space-y-1 text-sm pl-2">
							<DetailRow
								icon={IconRadioModule}
								label="Model"
								value={radioModuleModelName[device.radioModule.model ?? -1] ?? 'Unknown'}
							/>
							<DetailRow icon={IconNumber} label="Serial Number" value={device.radioModule.sn} />

							{#if device.readoutTime}
								<div class="divider my-1 mt-3 text-sm text-base-content/40">Radio Status</div>

								<!-- Matches Hekatron Genius Home: bit 0 (FmFault) + bit 3 (FmBatteryLowFault) -->
								{@const fmFault = !!((device.radioModule.radioStateMask ?? 0) & 0x09)}
								<DetailRow icon={IconCheckCircle} label="Radio Status">
									{#if fmFault}
										<span class="inline-flex items-center gap-1">
											<IconError class="h-4 w-4 flex-shrink-0 text-error" />
											<span class="font-medium text-error">Fault</span>
										</span>
									{:else}
										<span class="inline-flex items-center gap-1">
											<IconCheckCircle class="h-4 w-4 flex-shrink-0 text-success" />
											<span class="font-medium">OK</span>
										</span>
									{/if}
								</DetailRow>

								<DetailRow icon={IconRadar} label="Radio State Flags" />

								{#each RADIO_STATE_FLAGS as flag, i}
									{@const active = !!(((device.radioModule.radioStateMask ?? 0) >> i) & 1)}
									<div class="flex items-center gap-2 pl-6">
										{#if active && flag.warn}
											<IconWarning class="h-3.5 w-3.5 flex-shrink-0 text-warning" />
											<span class="text-xs text-warning">{flag.name}</span>
										{:else if active}
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
									<DetailRow
										icon={IconSignal}
										label="Interference"
										value={`${device.radioModule.radioInterference}%`}
									/>
								{/if}

								<div class="divider my-1 mt-3 text-sm text-base-content/40">Alarm Line</div>

								{#if device.radioModule.lineId}
									<DetailRow icon={IconHash} label="Line ID" value={device.radioModule.lineId} />
								{/if}
								{#if device.radioModule.lineCharacter || device.radioModule.lineNumber != null}
									<DetailRow
										icon={IconLetterCase}
										label="Line"
										value={`${device.radioModule.lineCharacter ?? '?'}.${device.radioModule.lineNumber ?? '?'}`}
									/>
								{/if}

								<DetailRow icon={IconToggleRightFilled} label="DIP Switch Config" />

								{#each switchStates as { name, active }}
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
