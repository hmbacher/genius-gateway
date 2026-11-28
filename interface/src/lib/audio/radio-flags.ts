/**
 * Bit masks of the two radio module bit fields of an acoustic readout.
 *
 * The values are those of the Hekatron apps (Genius Control and Genius Home). Both apps read
 * the byte as an 8-digit binary string, most significant bit first - the last digit is bit 0.
 */

/** Radio state mask - readout byte 20 (`radioStateMask`). */
export const RADIO_STATE = {
	RemoteAlarm: 0x01,
	RadioLinkError: 0x02,
	RemoteError: 0x04,
	RemoteBattLow: 0x08,
	FmBatteryLowFault: 0x10,
	Selftest: 0x20,
	TransmissionRangeTest: 0x40,
	FmFault: 0x80
} as const;

/**
 * DIP switch mask - readout byte 30 (`radioSwitchMask`). Bits 6 and 7 are not used; the
 * Hekatron apps reject a readout in which one of them is set.
 */
export const RADIO_SWITCH = {
	SuppressWarnings: 0x01,
	SuppressAlarms: 0x02,
	SendCollectiveAlarm: 0x04,
	ReceiveCollectiveAlarm: 0x08,
	RadioLinkSupervision: 0x10,
	ReducedTransmittingPower: 0x20
} as const;

/** Radio state bits shown as a fault of the radio module itself ("Radio Status: Fault"). */
export const RADIO_FAULT_MASK = RADIO_STATE.FmFault | RADIO_STATE.FmBatteryLowFault;
