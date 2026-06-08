<script lang="ts">
	import IconRevert from '~icons/tabler/arrow-back-up';

	interface Props {
		/** Whether the associated field differs from its saved value. */
		dirty: boolean;
		/** Reset the field back to its saved value. */
		onrevert: () => void;
		/** Accessible label / tooltip for the revert control. */
		title?: string;
	}

	let { dirty, onrevert, title = 'Revert change' }: Props = $props();
</script>

{#if dirty}
	<button
		type="button"
		class="text-error tooltip tooltip-left flex shrink-0 cursor-pointer items-center"
		data-tip={title}
		aria-label={title}
		onclick={() => onrevert()}
	>
		<IconRevert class="h-5 w-5" />
	</button>
{/if}
