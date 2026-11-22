<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import ProgressDialog from '$lib/components/ProgressDialog.svelte';
	import type { HaEntityRenameStatus } from '$lib/types/models';
	import { renamedDetectors } from '$lib/genius/entityRename';
	import HomeAssistant from '~icons/tabler/smart-home';
	import Send from '~icons/tabler/send';
	import Cancel from '~icons/tabler/x';

	interface Props extends ModalProps {
		/** Live state of the rename cycle; updated from the `ha-entity-rename` WebSocket event. */
		status: HaEntityRenameStatus;
	}

	let { isOpen, status, ...modalProps }: Props = $props();

	type View = {
		phase: 'pending' | 'progress' | 'success' | 'error';
		message: string;
		stepLabel?: string;
		successDetails?: string;
		errorDetails?: string;
	};

	// Who is renamed, by location. The dialog can appear without the user having asked for a
	// rename on this page (import, option switched on, catch-up), so it has to say who and why.
	const detectors = $derived(renamedDetectors(status.count, status.detectors));

	const view: View = $derived.by(() => {
		switch (status.state) {
			case 'removing':
				return {
					phase: 'progress',
					message: `Removing the entities of ${detectors} from Home Assistant...`,
					stepLabel: 'Step 1 of 3'
				};
			case 'waiting':
				return {
					phase: 'progress',
					message: 'Waiting for Home Assistant to drop the old entities...',
					stepLabel: 'Step 2 of 3'
				};
			case 'publishing':
				return {
					phase: 'progress',
					message: 'Publishing the entities with their new IDs...',
					stepLabel: 'Step 3 of 3'
				};
			case 'sent':
				// Home Assistant does not acknowledge discovery messages, so "sent" is all
				// the gateway knows - never present this as a confirmed rename.
				return {
					phase: 'success',
					message: 'Sent to Home Assistant',
					successDetails: `Entity IDs in Home Assistant follow the location of a smoke detector. The new entity IDs of ${detectors} were sent. Home Assistant does not confirm the change - check the entity IDs there.`
				};
			case 'failed':
				return {
					phase: 'error',
					message: 'Entity IDs not updated',
					errorDetails:
						'The MQTT connection was lost before everything was sent. The gateway repeats the update on the next connect.'
				};
			default:
				return { phase: 'pending', message: 'Preparing...' };
		}
	});
</script>

<ProgressDialog
	{isOpen}
	{...modalProps}
	title="Updating Entity IDs"
	titleIcon={HomeAssistant}
	phase={view.phase}
	message={view.message}
	stepLabel={view.stepLabel}
	successDetails={view.successDetails}
	successIcon={Send}
	successTone="primary"
	errorDetails={view.errorDetails}
	blockCloseDuringProgress={false}
	cancelButton={{
		label: 'Hide',
		icon: Cancel,
		class: 'btn-ghost',
		handler: () => modals.close()
	}}
	successButton={{
		label: 'Close',
		icon: Cancel,
		class: 'btn-primary',
		handler: () => modals.close()
	}}
	errorButtons={[
		{ label: 'Close', icon: Cancel, class: 'btn-primary', handler: () => modals.close() }
	]}
/>
