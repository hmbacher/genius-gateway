<script lang="ts">
	import { modals } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Check from '~icons/tabler/check';
	import IconInfo from '~icons/tabler/info-circle';
	import IconWarning from '~icons/tabler/alert-triangle';
	import IconError from '~icons/tabler/alert-circle';

	// provided by <Modals />

	type Variant = 'info' | 'warning' | 'error';

	interface Props {
		isOpen: boolean;
		title: string;
		message: string;
		onDismiss: any;
		variant?: Variant;
		dismiss?: any;
	}

	const {
		isOpen,
		title,
		message,
		onDismiss,
		variant = 'warning',
		dismiss = { label: 'Dismiss', icon: Check }
	}: Props = $props();

	const variantConfig: Record<Variant, { icon: any; iconClass: string; btnClass: string }> = {
		info:    { icon: IconInfo,    iconClass: 'text-info',    btnClass: 'btn btn-info text-info-content'       },
		warning: { icon: IconWarning, iconClass: 'text-warning', btnClass: 'btn btn-warning text-warning-content' },
		error:   { icon: IconError,   iconClass: 'text-error',   btnClass: 'btn btn-error text-error-content'     },
	};

	const cfg = $derived(variantConfig[variant]);
</script>

{#if isOpen}
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg"
		>
			<h2 class="text-base-content flex items-center gap-2 text-start text-2xl font-bold">
				<cfg.icon class="h-7 w-7 flex-shrink-0 {cfg.iconClass}" />{title}
			</h2>
			<div class="divider my-2"></div>
			<p class="text-base-content mb-1 text-start">{@html message}</p>
			<div class="divider my-2"></div>
			<div class="flex justify-end gap-2">
				<button
					class="{cfg.btnClass} inline-flex items-center"
					onclick={onDismiss}
					><dismiss.icon class="h-5 w-5" /><span>{dismiss.label}</span></button
				>
			</div>
		</div>
	</div>
{/if}
