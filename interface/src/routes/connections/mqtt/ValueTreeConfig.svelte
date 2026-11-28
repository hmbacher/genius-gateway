<script lang="ts">
	import { onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import FieldError from '$lib/components/FieldError.svelte';
	import { isMQTTTopicPath } from '$lib/utils/validators';
	import IconTree from '~icons/tabler/binary-tree';
	import Info from '~icons/tabler/info-circle';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';

	// The value tree shares its settings object with the simple alarm publishing
	// (/rest/alarm-publishing). This card reads and writes only its own two fields.
	type ValueTreeSettings = {
		treeEnabled: boolean;
		treeBaseTopic: string;
	};

	const maxTopicPathLength = 64;
	const defaultRoot = 'genius-gateway/<mac>';

	const f = createDirtyState<ValueTreeSettings>({ treeEnabled: false, treeBaseTopic: '' });
	let settingsLoaded = $state(false);

	function ownFields(settings: Partial<ValueTreeSettings>): ValueTreeSettings {
		return {
			treeEnabled: settings.treeEnabled === true,
			treeBaseTopic: settings.treeBaseTopic ?? ''
		};
	}

	function headers() {
		return {
			Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
			'Content-Type': 'application/json'
		};
	}

	async function getSettings() {
		try {
			const response = await fetch('/rest/alarm-publishing', { method: 'GET', headers: headers() });
			f.reset(ownFields(await response.json()));
			settingsLoaded = true;
		} catch (error) {
			console.error('Error:', error);
		}
	}

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getSettings();
		}
	});

	// Empty means the default root
	const rootTopicError = $derived(
		f.current.treeBaseTopic !== '' && !isMQTTTopicPath(f.current.treeBaseTopic, maxTopicPathLength)
	);

	async function postSettings() {
		if (rootTopicError) return;
		try {
			const response = await fetch('/rest/alarm-publishing', {
				method: 'POST',
				headers: headers(),
				body: JSON.stringify(ownFields(f.current))
			});
			if (response.status == 200) {
				notifications.success('Value tree settings updated.', 3000);
				f.reset(ownFields(await response.json()));
			} else {
				notifications.error('Updating value tree settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}
</script>

<Collapsible open={false} class="shadow-lg" isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
	{#snippet icon()}
		<IconTree class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>MQTT Value Tree</span>
	{/snippet}
	{#if !settingsLoaded}
		<Spinner />
	{:else}
		<form
			class="fieldset"
			onsubmit={(e) => {
				e.preventDefault();
				postSettings();
			}}
			novalidate
		>
			<div class="alert alert-info my-2 shadow-lg">
				<Info class="h-6 w-6 shrink-0 stroke-current" />
				<span>
					For smart home systems other than Home Assistant: every value of every smoke detector and
					alarm line is published to a topic of its own, as plain text.
				</span>
			</div>
			<div class="grid w-full grid-cols-1 content-center gap-x-4 gap-y-2">
				<div class="flex items-center w-full mt-2">
					<div
						class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty(
							'treeEnabled'
						)
							? 'w-1 mr-2'
							: 'w-0'}"
					></div>
					<label class="label cursor-pointer justify-start gap-2 items-center">
						<input
							type="checkbox"
							class="toggle toggle-primary shrink-0"
							bind:checked={f.current.treeEnabled}
						/>
						<div class="flex items-center gap-2">
							<span>Enable MQTT value tree</span>
							<DirtyMarker
								dirty={f.isDirty('treeEnabled')}
								onrevert={() => f.revert('treeEnabled')}
							/>
						</div>
					</label>
				</div>
				<div>
					<label class="label" for="treeBaseTopic">Root Topic</label>
					<DirtyField
						dirty={f.isDirty('treeBaseTopic')}
						onrevert={() => f.revert('treeBaseTopic')}
					>
						<input
							type="text"
							placeholder={defaultRoot}
							class="input w-full pr-10 {rootTopicError ? 'border-error border-2' : ''}"
							bind:value={f.current.treeBaseTopic}
							id="treeBaseTopic"
							max={maxTopicPathLength}
						/>
					</DirtyField>
					<FieldError
						show={rootTopicError}
						message="Topic must be 1–{maxTopicPathLength} characters (a–z, A–Z, 0–9, -, _, ., /). No leading/trailing slashes."
					/>
					<span class="text-sm text-base-content/60"
						>Leave empty for <code>{defaultRoot}</code> (the gateway's MAC address without colons).</span
					>
				</div>
			</div>
			<div class="divider mb-2 mt-0"></div>
			<div class="mx-4 flex flex-wrap justify-end gap-2">
				<button class="btn btn-primary" type="submit" disabled={!f.anyDirty || rootTopicError}
					>Apply Settings</button
				>
			</div>
		</form>
	{/if}
</Collapsible>
