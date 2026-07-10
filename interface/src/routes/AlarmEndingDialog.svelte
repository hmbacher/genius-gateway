<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import BellOff from '~icons/tabler/bell-off';

	// provided by <Modals />

	interface Props extends ModalProps {
		title: string;
		onSubmit: (time: number) => void;
	}

	let { isOpen, title, onSubmit }: Props = $props();

	let AlarmBlockingTimesOptions = [
		{
			value: 0,
			text: `No blocking`
		},
		{
			value: 5 * 60,
			text: `5 minutes`
		},
		{
			value: 10 * 60,
			text: `10 minutes`
		},
		{
			value: 15 * 60,
			text: `15 minutes`
		},
		{
			value: 30 * 60,
			text: `30 minutes`
		},
		{
			value: 45 * 60,
			text: `45 minutes`
		},
		{
			value: 60 * 60,
			text: `60 minutes`
		}
	];

	let time = $state(0); // Default to no blocking time

	const titleId = `alarm-ending-title-${Math.random().toString(36).slice(2)}`;
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
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg md:w-[28rem]"
		>
			<h2
				id={titleId}
				class="text-base-content flex items-center gap-2 text-start text-2xl font-bold"
			>
				<BellOff class="text-primary h-7 w-7 flex-shrink-0" />{title}
			</h2>
			<div class="divider my-2"></div>
			<form
				class="form-control text-base-content mb-1 w-full"
				onsubmit={(e) => {
					e.preventDefault();
					onSubmit(time);
				}}
				novalidate
			>
				<div>
					<label class="label" for="radioModuleModel">
						<span class="label-text">How long shall new alarms be ignored?</span>
					</label>
					<select class="select w-full" id="radioModuleModel" bind:value={time}>
						{#each AlarmBlockingTimesOptions as timeOption}
							<option value={timeOption.value}>
								{timeOption.text}
							</option>
						{/each}
					</select>
				</div>

				<div class="divider my-2"></div>
				<div class="flex justify-end gap-2">
					<button
						class="btn btn-neutral text-neutral-content inline-flex items-center"
						onclick={() => {
							modals.close(1);
						}}
						type="button"
					>
						<Cancel class="mr-2 h-5 w-5" />
						<span>Cancel</span>
					</button>
					<button class="btn btn-primary inline-flex items-center" type="submit">
						<BellOff class="mr-2 h-5 w-5" />
						<span>End alarms</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
