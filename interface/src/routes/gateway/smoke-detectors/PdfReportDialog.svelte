<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { untrack } from 'svelte';
	import ProgressDialog from '$lib/components/ProgressDialog.svelte';
	import { notifications } from '$lib/components/toasts/notifications';

	interface Props extends ModalProps {
		/** Runs the report generation. `onProgress` updates the dialog's step label.
		 *  Caller-owned so the parent can keep its fetch logic (bearer token,
		 *  REST endpoints) where it already lives. */
		task: (onProgress: (step: string) => void) => Promise<void>;
	}

	let { isOpen, task, ...modalProps }: Props = $props();

	let stepLabel = $state('Starting');

	async function run() {
		try {
			await task((step) => {
				stepLabel = step;
			});
		} catch (error) {
			console.error('Error generating report:', error);
			notifications.error('Failed to generate PDF report.', 3000);
		} finally {
			modals.close();
		}
	}

	// Kick off the task once the dialog mounts. `untrack` prevents the effect
	// from re-running if task or stepLabel are ever reassigned mid-flight.
	$effect(() => {
		untrack(() => run());
	});
</script>

<ProgressDialog
	{isOpen}
	{...modalProps}
	title="Generating PDF Report"
	phase="progress"
	message={stepLabel}
	blockCloseDuringProgress={false}
/>
