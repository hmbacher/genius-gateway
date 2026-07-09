import type { GeniusDevice, ReportSettings } from '$lib/types/models';
import {
	GeniusSmokeDetector,
	GeniusRadioModule,
	GeniusDeviceRegistration,
	GeniusAlarmEnding
} from '$lib/types/enums';
import { formatDate, formatDateTime, formatAge } from '$lib/utils/formatDate';
import {
	getSmokeDetectorFaults,
	getRadioModuleFaults,
	hasReadout,
	isStaleReadout
} from '$lib/utils/deviceStatus';
import smokeDetector2xlSvgRaw from '$lib/assets/icons/smoke-detector-2xl.svg?raw';
import logoSvgRaw from '$lib/assets/logo.svg?raw';

export type GatewayInfo = {
	hostname: string;
	firmwareVersion: string;
};

// ─── SVG assets for PDF ───────────────────────────────────────────────────────

const DEVICE_ICON_SVG = smokeDetector2xlSvgRaw.replace(/currentColor/g, '#374151');

// Strip CSS <style> blocks (pdfmake ignores them, leaving class-based fills at
// their default black/red). Then inline the correct fills via attributes.
const LOGO_SVG = logoSvgRaw
	.replace(/<style[\s\S]*?<\/style>/g, '')
	.replace(/class="st0"/g, 'fill="#2ca089"')
	.replace(/class="st1"/g, 'fill="#ffffff"');

// ─── lookup tables (mirrors DeviceDetailsDialog) ────────────────────────────

const SD_MODEL_NAME: Record<number, string> = {
	[GeniusSmokeDetector.Unknown]: 'Unknown',
	[GeniusSmokeDetector.GeniusH]: 'Genius H',
	[GeniusSmokeDetector.GeniusHx]: 'Genius Hx',
	[GeniusSmokeDetector.GeniusPlus]: 'Genius Plus',
	[GeniusSmokeDetector.GeniusPlusX]: 'Genius Plus X'
};

const RM_MODEL_NAME: Record<number, string> = {
	[GeniusRadioModule.Unknown]: 'Unknown',
	[GeniusRadioModule.None]: 'None',
	[GeniusRadioModule.FmBasis]: 'FM.Basis',
	[GeniusRadioModule.FmPro]: 'FM.Pro',
	[GeniusRadioModule.FmMcp]: 'FM.MCP',
	[GeniusRadioModule.FmBasisX]: 'FM.Basis X',
	[GeniusRadioModule.FmProX]: 'FM.Pro X'
};

const REGISTRATION_NAME: Record<number, string> = {
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

const RADIO_STATE_FLAGS: { name: string; inactiveText: string; neutral: boolean; warn?: boolean }[] = [
	{ name: 'FM Fault', inactiveText: 'No FM Fault', neutral: false },
	{ name: 'Range Test active', inactiveText: 'Range Test not active', neutral: true },
	{ name: 'Self-Test active', inactiveText: 'Self-Test not active', neutral: true },
	{ name: 'FM Battery Low', inactiveText: 'FM Battery not low', neutral: false },
	{ name: 'Remote Battery Low', inactiveText: 'Remote Battery not low', neutral: false, warn: true },
	{ name: 'Remote Error', inactiveText: 'No Remote Error', neutral: false, warn: true },
	{ name: 'Radio Link Error', inactiveText: 'No Radio Link Error', neutral: false },
	{ name: 'Remote Alarm', inactiveText: 'Remote Alarm not active', neutral: true }
];

const RADIO_SWITCH_CONFIG: [number, string][] = [
	[2, 'Suppress Warnings'],
	[3, 'Suppress Alarms'],
	[4, 'Send Collective Alarm'],
	[5, 'Receive Collective Alarm'],
	[6, 'Radio Link Supervision'],
	[7, 'Reduced TX Power']
];

// ─── color tokens ─────────────────────────────────────────────────────────────

const C_ERROR = '#dc2626';
const C_WARN = '#d97706';
const C_OK = '#16a34a';
const C_MUTED = '#9ca3af';
const C_SECTION_BG = '#f3f4f6';
const C_FAULT_BG = '#fee2e2';
const C_FAULT_BORDER = '#fca5a5';
const C_WARN_BG = '#fffbeb';
const C_WARN_BORDER = '#fcd34d';

// ─── helpers ─────────────────────────────────────────────────────────────────

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type PdfContent = Record<string, any>;

function formatDuration(startTime: Date, endTime: Date): string {
	const ms = endTime.getTime() - startTime.getTime();
	if (ms < 0) return '-';
	const totalSeconds = Math.floor(ms / 1000);
	const h = Math.floor(totalSeconds / 3600);
	const m = Math.floor((totalSeconds % 3600) / 60);
	const s = totalSeconds % 60;
	if (h > 0) return `${h}h ${m}m`;
	if (m > 0) return `${m}m ${s}s`;
	return `${s}s`;
}

function alarmEndingReasonText(reason: number): string {
	switch (reason) {
		case GeniusAlarmEnding.BySmokeDetector:
			return 'Automatic';
		case GeniusAlarmEnding.ByManual:
			return 'Manual';
		case GeniusAlarmEnding.ByImport:
			return 'Import';
		default:
			return '-';
	}
}

function formatISODatetime(d: Date): string {
	const pad = (n: number) => String(n).padStart(2, '0');
	return (
		`${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ` +
		`${pad(d.getHours())}:${pad(d.getMinutes())}`
	);
}

function formatISODate(d: Date): string {
	const pad = (n: number) => String(n).padStart(2, '0');
	return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
}

// ─── style helpers ────────────────────────────────────────────────────────────

function sectionHeader(text: string): PdfContent {
	return {
		table: { widths: ['*'], body: [[{ text, bold: true, fontSize: 10, margin: [8, 2, 0, 2] }]] },
		layout: { fillColor: () => C_SECTION_BG, hLineWidth: () => 0, vLineWidth: () => 0 },
		margin: [0, 10, 0, 4]
	};
}

/** labelWidth: narrow inside 2-col right pane (default 120, use 90 for right column).
 *  leftMargin: set to 0 when row is already inside a padded container (e.g. property block). */
function detailRow(
	label: string,
	value: string,
	valueColor?: string,
	labelWidth = 120,
	leftMargin = 8
): PdfContent {
	return {
		columns: [
			{ text: label, width: labelWidth, fontSize: 9, color: C_MUTED },
			{ text: value, fontSize: 9, bold: true, color: valueColor ?? '#111827' }
		],
		margin: [leftMargin, 1, 0, 1]
	};
}

/** Tabler-style SVG icons for flag indicators, matching the frontend's icon set. */
type FlagIcon = 'check' | 'error' | 'warn' | 'muted-x';

function flagIconSvg(type: FlagIcon): string {
	const c =
		type === 'error' ? C_ERROR : type === 'warn' ? C_WARN : type === 'check' ? C_OK : C_MUTED;
	const base = `xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="${c}" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"`;
	switch (type) {
		case 'check':
			// tabler/check
			return `<svg ${base}><polyline points="4 13 9 18 20 7"/></svg>`;
		case 'error':
			// tabler/circle-x
			return `<svg ${base}><circle cx="12" cy="12" r="9"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>`;
		case 'warn':
			// tabler/alert-triangle
			return `<svg ${base}><path d="M10.5 4.5L2 20h20L13.5 4.5a1.74 1.74 0 0 0-3 0z"/><line x1="12" y1="10" x2="12" y2="14"/><circle cx="12" cy="17.5" r="0.5" fill="${c}"/></svg>`;
		case 'muted-x':
			// tabler/x
			return `<svg ${base}><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>`;
	}
}

function flagRow(icon: FlagIcon, label: string, textColor: string, italics = false): PdfContent {
	return {
		columns: [
			{ svg: flagIconSvg(icon), width: 11, height: 11 },
			{ text: label, fontSize: 8, color: textColor, italics }
		],
		columnGap: 4,
		margin: [8, 1, 0, 1]
	};
}

function faultCallout(faults: string[]): PdfContent {
	return {
		table: {
			widths: ['*'],
			body: [
				[
					{
						stack: [
							{
								text: 'Faults detected',
								bold: true,
								fontSize: 10,
								color: C_ERROR,
								margin: [0, 0, 0, 4]
							},
							...faults.map((f) => ({ text: `• ${f}`, fontSize: 9, color: C_ERROR }))
						],
						margin: [8, 6, 8, 6]
					}
				]
			]
		},
		layout: {
			fillColor: () => C_FAULT_BG,
			hLineColor: () => C_FAULT_BORDER,
			vLineColor: () => C_FAULT_BORDER,
			hLineWidth: () => 1,
			vLineWidth: () => 1
		},
		margin: [0, 0, 0, 10]
	};
}

function warningCallout(message: string): PdfContent {
	return {
		table: {
			widths: ['*'],
			body: [[{ text: message, bold: true, fontSize: 10, color: C_WARN, margin: [8, 6, 8, 6] }]]
		},
		layout: {
			fillColor: () => C_WARN_BG,
			hLineColor: () => C_WARN_BORDER,
			vLineColor: () => C_WARN_BORDER,
			hLineWidth: () => 1,
			vLineWidth: () => 1
		},
		margin: [0, 0, 0, 10]
	};
}

// ─── per-device page ──────────────────────────────────────────────────────────

function buildDevicePage(device: GeniusDevice, index: number, total: number): PdfContent[] {
	const sd = device.smokeDetector;
	const rm = device.radioModule;
	const hasRm = rm.model !== GeniusRadioModule.None && rm.model !== undefined && (rm.sn ?? 0) > 0;
	const sdReadout = hasReadout(device);
	const stale = isStaleReadout(device);
	const sdFaults = sdReadout ? getSmokeDetectorFaults(sd) : [];
	const rmFaults = sdReadout && hasRm ? getRadioModuleFaults(rm) : [];
	const allFaults = [...sdFaults, ...rmFaults];

	const blocks: PdfContent[] = [];

	// ── Device header (full-width) with smoke detector icon ──
	// smoke-detector-2xl viewBox: 506.483 × 360.58 → aspect ≈ 1.404
	blocks.push({
		columns: [
			{
				width: 48,
				stack: [{ svg: DEVICE_ICON_SVG, width: 42, height: 30 }],
				margin: [0, 2, 0, 0]
			},
			{
				stack: [
					{ text: device.location, fontSize: 16, bold: true },
					{ text: `Smoke Detector ${index + 1} of ${total}`, fontSize: 9, color: C_MUTED }
				]
			},
			{ text: `SN ${sd.sn}`, fontSize: 10, color: C_MUTED, alignment: 'right', margin: [0, 5, 0, 0] }
		],
		margin: [0, 0, 0, 8]
	});

	// ── Status banners (full-width) ──
	if (!sdReadout) {
		blocks.push(warningCallout('No readout data available for this device.'));
	} else if (stale) {
		blocks.push(warningCallout('Readout data is stale - last readout was more than 24 hours ago.'));
	}

	if (allFaults.length > 0) {
		blocks.push(faultCallout(allFaults));
	}

	// ── Left column: General + Smoke Detector ─────────────────────────────────

	const leftCol: PdfContent[] = [];

	leftCol.push(sectionHeader('General'));
	leftCol.push(detailRow('Location', device.location));
	leftCol.push(detailRow('Registration', REGISTRATION_NAME[device.registration] ?? 'Unknown'));
	if (device.readoutTime) {
		leftCol.push(
			detailRow(
				'Last Readout',
				`${formatDateTime(device.readoutTime)} (${formatAge(device.readoutTime)} ago)`,
				stale ? C_WARN : C_OK
			)
		);
	} else {
		leftCol.push(detailRow('Last Readout', 'Never', C_WARN));
	}

	leftCol.push(sectionHeader('Smoke Detector'));
	leftCol.push(detailRow('Model', SD_MODEL_NAME[sd.model ?? -1] ?? 'Unknown'));
	leftCol.push(detailRow('Serial Number', String(sd.sn)));
	leftCol.push(detailRow('Production Date', formatDate(sd.productionDate)));
	leftCol.push(detailRow('Age', formatAge(sd.productionDate)));

	if (sdReadout) {
		leftCol.push({
			text: 'Status',
			fontSize: 8,
			color: C_MUTED,
			margin: [8, 6, 0, 2],
			italics: true
		});
		leftCol.push(
			detailRow('Detector Status', sd.deviceFault ? 'Fault' : 'OK', sd.deviceFault ? C_ERROR : C_OK)
		);
		leftCol.push(
			detailRow(
				'Battery',
				sd.batteryLowFault ? 'Low' : 'OK',
				sd.batteryLowFault ? C_ERROR : C_OK
			)
		);
		leftCol.push(
			detailRow(
				'Dirt Forecast',
				sd.dirtForecastNegative ? 'Negative' : 'OK',
				sd.dirtForecastNegative ? C_ERROR : C_OK
			)
		);
		leftCol.push(detailRow('Chamber Drift', String(sd.driftState ?? 0)));
		const warrantySet = (sd.warrantyFlags ?? 0) > 0;
		leftCol.push(
			detailRow('Warranty', warrantySet ? 'Voided' : 'OK', warrantySet ? C_ERROR : C_OK)
		);
		leftCol.push({
			stack: WARRANTY_FLAG_NAMES.map((name, i) => {
				const active = !!(((sd.warrantyFlags ?? 0) >> i) & 1);
				return active
					? flagRow('error', name, C_ERROR)
					: flagRow('check', `${name} not set`, C_MUTED, true);
			}),
			margin: [0, 0, 0, 4]
		});

		leftCol.push({
			text: 'Statistics',
			fontSize: 8,
			color: C_MUTED,
			margin: [8, 6, 0, 2],
			italics: true
		});
		leftCol.push(detailRow('Last Self-Test', formatDate(sd.lastSelftest)));
		leftCol.push(detailRow('Last Alarm', formatDate(sd.lastAlarm)));
		leftCol.push(detailRow('Alarms (total)', String(sd.alarmCountTotal ?? 0)));
		leftCol.push(detailRow('Alarms (3 months)', String(sd.alarmCountLast3Months ?? 0)));
		leftCol.push(detailRow('Deinstallations', String(sd.deinstallationCount ?? 0)));
		leftCol.push(detailRow('Storage Hours', String(sd.hoursInStorageMode ?? 0)));
	}

	// ── Right column: Radio Module ────────────────────────────────────────────

	const rightCol: PdfContent[] = [];
	const LW = 90; // label width for narrower right column

	rightCol.push(sectionHeader('Radio Module'));

	if (!hasRm) {
		rightCol.push({
			text: 'No radio module installed.',
			fontSize: 9,
			color: C_MUTED,
			italics: true,
			margin: [0, 2, 0, 4]
		});
	} else {
		rightCol.push(detailRow('Model', RM_MODEL_NAME[rm.model ?? -1] ?? 'Unknown', undefined, LW));
		rightCol.push(detailRow('Serial Number', String(rm.sn), undefined, LW));

		if (sdReadout) {
			const fmFault = !!((rm.radioStateMask ?? 0) & 0x09);
			rightCol.push(
				detailRow('Radio Status', fmFault ? 'Fault' : 'OK', fmFault ? C_ERROR : C_OK, LW)
			);
		}

		// Direct-link signal from the last ConfigCheckProbe range test. Independent of the
		// readout, so shown whenever a module exists. Mirrors SignalIndicator's three states
		// and dBm→quality thresholds.
		rightCol.push({
			text: 'Direct Link',
			fontSize: 8,
			color: C_MUTED,
			margin: [8, 6, 0, 2],
			italics: true
		});
		if (!rm.lastRangeTest) {
			rightCol.push(detailRow('Signal', 'Not range-tested yet', C_MUTED, LW));
		} else if ((rm.rssi ?? 0) < 0) {
			const dbm = rm.rssi as number;
			const quality =
				dbm >= -60 ? 'Excellent' : dbm >= -70 ? 'Good' : dbm >= -80 ? 'Fair' : 'Weak';
			rightCol.push(detailRow('Signal', `${dbm} dBm (${quality})`, C_OK, LW));
			rightCol.push(detailRow('Range Test', formatDateTime(rm.lastRangeTest), undefined, LW));
		} else {
			rightCol.push(detailRow('Signal', 'Out of direct range', C_MUTED, LW));
			rightCol.push(detailRow('Range Test', formatDateTime(rm.lastRangeTest), undefined, LW));
		}

		if (sdReadout) {
			rightCol.push({
				text: 'Radio State Flags',
				fontSize: 8,
				color: C_MUTED,
				margin: [8, 6, 0, 2],
				italics: true
			});
			rightCol.push({
				stack: RADIO_STATE_FLAGS.map((flag, i) => {
					const active = !!(((rm.radioStateMask ?? 0) >> i) & 1);
					if (active) {
						return flag.warn
							? flagRow('warn', flag.name, C_WARN)
							: flagRow('error', flag.name, C_ERROR);
					} else {
						return flag.neutral
							? flagRow('muted-x', flag.inactiveText, C_MUTED, true)
							: flagRow('check', flag.inactiveText, C_MUTED, true);
					}
				}),
				margin: [0, 0, 0, 4]
			});

			if ((rm.radioInterference ?? 0) > 0) {
				rightCol.push(detailRow('Interference', `${rm.radioInterference}%`, undefined, LW));
			}

			if (rm.lineId) {
				rightCol.push({
					text: 'Alarm Line',
					fontSize: 8,
					color: C_MUTED,
					margin: [8, 6, 0, 2],
					italics: true
				});
				rightCol.push(detailRow('Line ID', String(rm.lineId), undefined, LW));
				if (rm.lineCharacter) {
					rightCol.push(
						detailRow('Line', `${rm.lineCharacter}.${rm.lineNumber ?? '?'}`, undefined, LW)
					);
				}
			}

			rightCol.push({
				text: 'DIP Switch Config',
				fontSize: 8,
				color: C_MUTED,
				margin: [8, 6, 0, 2],
				italics: true
			});
			rightCol.push({
				stack: RADIO_SWITCH_CONFIG.map(([bit, name]) => {
					const active = !!(((rm.radioSwitchMask ?? 0) >> bit) & 1);
					return active
						? flagRow('check', `${name} is on`, '#374151')
						: flagRow('muted-x', `${name} is off`, C_MUTED, true);
				}),
				margin: [0, 0, 0, 4]
			});
		}
	}

	// ── 2-column section ──
	blocks.push({
		columnGap: 14,
		columns: [
			{ width: 255, stack: leftCol },
			{ stack: rightCol }
		]
	});

	// ── Alarm Log (full-width) ──
	blocks.push(sectionHeader('Alarm Log'));

	if (device.alarms.length === 0) {
		blocks.push({
			text: 'No alarms recorded.',
			fontSize: 9,
			color: C_MUTED,
			italics: true,
			margin: [0, 2, 0, 4]
		});
	} else {
		const alarmRows = [...device.alarms].reverse().map((alarm) => {
			const active = alarm.endingReason === GeniusAlarmEnding.AlarmActive;
			return [
				{ text: formatDateTime(alarm.startTime), fontSize: 8 },
				{
					text: active ? '-' : formatDateTime(alarm.endTime),
					fontSize: 8,
					color: active ? C_MUTED : '#111827'
				},
				{
					text: active ? 'Active' : formatDuration(alarm.startTime, alarm.endTime),
					fontSize: 8,
					color: active ? C_ERROR : '#111827'
				},
				{ text: alarmEndingReasonText(alarm.endingReason), fontSize: 8 }
			];
		});

		blocks.push({
			table: {
				headerRows: 1,
				widths: ['*', '*', 60, 70],
				body: [
					[
						{ text: 'Start', bold: true, fontSize: 8, fillColor: C_SECTION_BG },
						{ text: 'End', bold: true, fontSize: 8, fillColor: C_SECTION_BG },
						{ text: 'Duration', bold: true, fontSize: 8, fillColor: C_SECTION_BG },
						{ text: 'Ending Reason', bold: true, fontSize: 8, fillColor: C_SECTION_BG }
					],
					...alarmRows
				]
			},
			layout: 'lightHorizontalLines',
			margin: [0, 0, 0, 8]
		});
	}

	return blocks;
}

// ─── overview page ────────────────────────────────────────────────────────────

function buildOverviewPage(
	devices: GeniusDevice[],
	reportSettings: ReportSettings,
	now: Date
): PdfContent[] {
	const blocks: PdfContent[] = [];

	// Title header: GG logo + title + date
	blocks.push({
		columnGap: 14,
		columns: [
			{
				width: 80,
				stack: [{ svg: LOGO_SVG, width: 70, height: 44 }]
			},
			{
				stack: [
					{ text: 'Smoke Detector Report', fontSize: 22, bold: true },
					{ text: `Generated: ${formatISODatetime(now)}`, fontSize: 9, color: C_MUTED }
				],
				margin: [0, 6, 0, 0]
			}
		],
		margin: [0, 0, 0, 16]
	});

	// Property block
	const hasProperty =
		reportSettings.propertyName || reportSettings.propertyAddress || reportSettings.customerName;
	if (hasProperty) {
		const propertyRows: PdfContent[] = [];
		if (reportSettings.propertyName)
			propertyRows.push(detailRow('Property', reportSettings.propertyName, undefined, 120, 0));
		if (reportSettings.propertyAddress)
			propertyRows.push(detailRow('Address', reportSettings.propertyAddress, undefined, 120, 0));
		if (reportSettings.customerName)
			propertyRows.push(detailRow('Customer', reportSettings.customerName, undefined, 120, 0));

		blocks.push({
			table: {
				widths: ['*'],
				body: [[{ stack: propertyRows, margin: [8, 6, 8, 6] }]]
			},
			layout: {
				fillColor: () => C_SECTION_BG,
				hLineWidth: () => 0,
				vLineWidth: () => 0
			},
			margin: [0, 0, 0, 16]
		});
	}

	// Summary line
	const totalAlarms = devices.reduce((sum, d) => sum + d.alarms.length, 0);
	const devicesWithFaults = devices.filter(
		(d) =>
			d.readoutTime &&
			(getSmokeDetectorFaults(d.smokeDetector).length > 0 ||
				getRadioModuleFaults(d.radioModule).length > 0)
	).length;
	const devicesMissingReadout = devices.filter((d) => !d.readoutTime).length;

	blocks.push({
		columns: [
			{ text: `${devices.length} device${devices.length !== 1 ? 's' : ''}`, fontSize: 10, bold: true },
			{ text: `${totalAlarms} alarm${totalAlarms !== 1 ? 's' : ''}`, fontSize: 10, bold: true },
			{
				text: `${devicesWithFaults} with fault${devicesWithFaults !== 1 ? 's' : ''}`,
				fontSize: 10,
				bold: true,
				color: devicesWithFaults > 0 ? C_ERROR : '#111827'
			},
			{
				text: `${devicesMissingReadout} without readout`,
				fontSize: 10,
				bold: true,
				color: devicesMissingReadout > 0 ? C_WARN : '#111827'
			}
		],
		margin: [0, 0, 0, 16]
	});

	// Overview table
	blocks.push(sectionHeader('Device Overview'));

	const tableRows = devices.map((device, i) => {
		const sd = device.smokeDetector;
		const rm = device.radioModule;
		const hasRm =
			rm.model !== GeniusRadioModule.None && rm.model !== undefined && (rm.sn ?? 0) > 0;
		const sdFaults = device.readoutTime ? getSmokeDetectorFaults(sd) : [];
		const rmFaults = device.readoutTime && hasRm ? getRadioModuleFaults(rm) : [];
		const hasFaults = sdFaults.length + rmFaults.length > 0;

		// Direct-link range-test signal (see buildDevicePage), condensed for the table.
		const signalCell = !hasRm
			? { text: '—', fontSize: 8, alignment: 'center', color: C_MUTED }
			: !rm.lastRangeTest
				? { text: 'Not tested', fontSize: 8, alignment: 'center', color: C_MUTED }
				: (rm.rssi ?? 0) < 0
					? { text: `${rm.rssi} dBm`, fontSize: 8, alignment: 'center', color: C_OK }
					: { text: 'Out of range', fontSize: 8, alignment: 'center', color: C_MUTED };

		return [
			{ text: String(i + 1), fontSize: 8, alignment: 'center' },
			{ text: device.location, fontSize: 8, bold: true },
			{
				stack: [
					{ text: SD_MODEL_NAME[sd.model ?? -1] ?? 'Unknown', fontSize: 8 },
					{ text: `SN ${sd.sn}`, fontSize: 7, color: C_MUTED }
				]
			},
			{
				stack: hasRm
					? [
							{ text: RM_MODEL_NAME[rm.model ?? -1] ?? 'Unknown', fontSize: 8 },
							{ text: `SN ${rm.sn}`, fontSize: 7, color: C_MUTED }
						]
					: [{ text: 'None', fontSize: 8, color: C_MUTED, italics: true }]
			},
			signalCell,
			{
				text: String(device.alarms.length),
				fontSize: 8,
				alignment: 'center',
				color: '#111827'
			},
			{
				text: !device.readoutTime
					? 'No readout'
					: hasFaults
						? 'Fault'
						: isStaleReadout(device)
							? 'Stale'
							: 'OK',
				fontSize: 8,
				alignment: 'center',
				color: !device.readoutTime
					? C_WARN
					: hasFaults
						? C_ERROR
						: isStaleReadout(device)
							? C_WARN
							: C_OK
			}
		];
	});

	blocks.push({
		table: {
			headerRows: 1,
			widths: [20, '*', '*', '*', 56, 36, 50],
			body: [
				[
					{ text: '#', bold: true, fontSize: 8, fillColor: C_SECTION_BG, alignment: 'center' },
					{ text: 'Location', bold: true, fontSize: 8, fillColor: C_SECTION_BG },
					{ text: 'Smoke Detector', bold: true, fontSize: 8, fillColor: C_SECTION_BG },
					{ text: 'Radio Module', bold: true, fontSize: 8, fillColor: C_SECTION_BG },
					{ text: 'Signal', bold: true, fontSize: 8, fillColor: C_SECTION_BG, alignment: 'center' },
					{ text: 'Alarms', bold: true, fontSize: 8, fillColor: C_SECTION_BG, alignment: 'center' },
					{ text: 'Status', bold: true, fontSize: 8, fillColor: C_SECTION_BG, alignment: 'center' }
				],
				...tableRows
			]
		},
		layout: 'lightHorizontalLines',
		margin: [0, 0, 0, 8]
	});

	return blocks;
}

// ─── pdfmake runtime loader ──────────────────────────────────────────────────

// pdfmake lives in `static/pdf/` (staged from node_modules by the `prebuild`
// npm script) and is fetched on demand via classic <script> tags. Going
// through ESM `import()` would let SvelteKit's single-bundle strategy inline
// ~1.9 MB of pdfmake + Roboto VFS into the SPA, blowing initial load up to
// ~20 s on the ESP32. See docs/development/frontend-bundles.md.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type PdfMakeGlobal = any;

let pdfMakeReady: Promise<PdfMakeGlobal> | null = null;

function loadScript(src: string): Promise<void> {
	return new Promise((resolve, reject) => {
		const existing = document.querySelector<HTMLScriptElement>(`script[src="${src}"]`);
		if (existing?.dataset.loaded === 'true') {
			resolve();
			return;
		}
		const s = existing ?? document.createElement('script');
		s.src = src;
		s.async = true;
		s.addEventListener('load', () => {
			s.dataset.loaded = 'true';
			resolve();
		});
		s.addEventListener('error', () => reject(new Error(`Failed to load ${src}`)));
		if (!existing) document.head.appendChild(s);
	});
}

async function loadPdfMake(): Promise<PdfMakeGlobal> {
	if (pdfMakeReady) return pdfMakeReady;
	pdfMakeReady = (async () => {
		// pdfmake.min.js must run first - it assigns `window.pdfMake`.
		// vfs_fonts.js then attaches the Roboto VFS via
		// pdfMake.addVirtualFileSystem (modern builds) or pdfMake.vfs (legacy).
		await loadScript('/pdf/pdfmake.min.js');
		await loadScript('/pdf/vfs_fonts.js');
		// eslint-disable-next-line @typescript-eslint/no-explicit-any
		const pdfMake = (window as any).pdfMake;
		if (!pdfMake) throw new Error('pdfmake failed to initialize');
		return pdfMake;
	})();
	return pdfMakeReady;
}

// ─── main export ─────────────────────────────────────────────────────────────

export async function generateSmokeDetectorReport(
	devices: GeniusDevice[],
	reportSettings: ReportSettings,
	gatewayInfo: GatewayInfo,
	onProgress?: (step: string) => void
): Promise<void> {
	// First-time load fetches ~1.9 MB of pdfmake + Roboto VFS from the ESP32,
	// typically 5–10 s over Wi-Fi STA. Subsequent calls resolve instantly via
	// the cached promise in loadPdfMake().
	onProgress?.('Loading PDF library');
	const pdfMake = await loadPdfMake();

	onProgress?.('Building report layout');
	const now = new Date();
	const overviewBlocks = buildOverviewPage(devices, reportSettings, now);
	const deviceBlocks: PdfContent[] = devices.flatMap((device, i) => [
		{ text: '', pageBreak: 'before' },
		...buildDevicePage(device, i, devices.length)
	]);

	const footerText = `Generated by ${gatewayInfo.hostname} · v${gatewayInfo.firmwareVersion}`;

	const docDefinition = {
		pageSize: 'A4',
		pageMargins: [40, 50, 40, 50],
		content: [...overviewBlocks, ...deviceBlocks],
		footer: (currentPage: number, pageCount: number) => ({
			margin: [40, 8],
			columns: [
				{
					columnGap: 6,
					columns: [
						{ svg: LOGO_SVG, width: 14, height: 9 },
						{ text: footerText, fontSize: 7, color: C_MUTED, margin: [0, 1, 0, 0] }
					]
				},
				{
					text: `Page ${currentPage} / ${pageCount}`,
					fontSize: 7,
					color: C_MUTED,
					alignment: 'right'
				}
			]
		}),
		fonts: {
			Roboto: {
				normal: 'Roboto-Regular.ttf',
				bold: 'Roboto-Medium.ttf',
				italics: 'Roboto-Italic.ttf',
				bolditalics: 'Roboto-MediumItalic.ttf'
			}
		},
		defaultStyle: {
			font: 'Roboto',
			fontSize: 10
		}
	};

	const filename = `smoke-detector-report-${formatISODate(now)}.pdf`;
	onProgress?.('Rendering PDF');
	pdfMake.createPdf(docDefinition).download(filename);
}
