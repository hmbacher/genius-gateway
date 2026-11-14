<script lang="ts">
	import { onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import FieldError from '$lib/components/FieldError.svelte';
	import { isMQTTTopicPath } from '$lib/utils/validators';
	import IconAlarm from '~icons/tabler/bell';
	import Info from '~icons/tabler/info-circle';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';

	type AlarmPublishingSettings = {
		alarmEnabled: boolean;
		alarmTopic: string;
		treeEnabled: boolean;
		treeBaseTopic: string;
	};

	const maxTopicPathLength = 64;
	const defaultSettings: AlarmPublishingSettings = {
		alarmEnabled: false,
		alarmTopic: 'smarthome/genius-gateway/alarm',
		treeEnabled: false,
		treeBaseTopic: ''
	};

	const f = createDirtyState<AlarmPublishingSettings>({ ...defaultSettings });
	let settingsLoaded = $state(false);

	async function getAlarmPublishingSettings() {
		try {
			const response = await fetch('/rest/alarm-publishing', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			f.reset(await response.json());
			settingsLoaded = true;
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getAlarmPublishingSettings();
		}
	});

	const alarmTopicError = $derived(!isMQTTTopicPath(f.current.alarmTopic, maxTopicPathLength));
	// The value tree root is optional: empty = default "genius-gateway/<mac>"
	const treeBaseTopicError = $derived(
		f.current.treeBaseTopic !== '' && !isMQTTTopicPath(f.current.treeBaseTopic, maxTopicPathLength)
	);

	function handleSubmit() {
		if (!alarmTopicError && !treeBaseTopicError) {
			postAlarmPublishingSettings();
		}
	}

	async function postAlarmPublishingSettings() {
		try {
			const response = await fetch('/rest/alarm-publishing', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});
			if (response.status == 200) {
				notifications.success('Alarm settings updated.', 3000);
				f.reset(await response.json());
			} else {
				notifications.error('Updating alarm settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}
</script>

<Collapsible open={false} class="shadow-lg" isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
	{#snippet icon()}
		<IconAlarm class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>Simple Alarm Publishing / Value Tree</span>
	{/snippet}
	{#if !settingsLoaded}
		<Spinner />
	{:else}
		<form
			class="fieldset"
			onsubmit={(e) => { e.preventDefault(); handleSubmit(); }}
			novalidate
		>
			<div class="alert alert-info my-2 shadow-lg">
				<Info class="h-6 w-6 shrink-0 stroke-current" />
				<span>As soon as a smoke detector is alarming, it will be published to a central MQTT topic.</span>
			</div>
			<div class="grid w-full grid-cols-1 content-center gap-x-4 gap-y-2">
				<div class="flex items-center w-full mt-2">
					<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('alarmEnabled') ? 'w-1 mr-2' : 'w-0'}"></div>
					<label class="label cursor-pointer justify-start gap-2 items-center">
						<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.alarmEnabled} />
						<div class="flex items-center gap-2">
							<span>Enable simple alarm publishing</span>
							<DirtyMarker dirty={f.isDirty('alarmEnabled')} onrevert={() => f.revert('alarmEnabled')} />
						</div>
					</label>
				</div>
				<div>
					<label class="label" for="alarmTopic">Alarm Topic</label>
					<DirtyField dirty={f.isDirty('alarmTopic')} onrevert={() => f.revert('alarmTopic')}>
						<input
							type="text"
							placeholder={`E.g. ${defaultSettings.alarmTopic}`}
							class="input w-full pr-10 {alarmTopicError ? 'border-error border-2' : ''}"
							bind:value={f.current.alarmTopic}
							id="alarmTopic"
							min="1"
							max={maxTopicPathLength}
							required
						/>
					</DirtyField>
					<FieldError show={alarmTopicError} message="Topic must be 1–{maxTopicPathLength} characters (a–z, A–Z, 0–9, -, _, ., /). No leading/trailing slashes." />
				</div>
			</div>
			<div class="divider my-2"></div>
				<div class="alert alert-info my-2 shadow-lg">
					<Info class="h-6 w-6 shrink-0 stroke-current" />
					<span>
						Generic value tree: every value of every smoke detector (alarm, location, serial numbers, last seen,
						signal strength, radio link fault, ...) is published to its own topic as plain
						<code>true</code>/<code>false</code>, a number or text. No JSON, independent of Home Assistant.
						Topics: <code>&lt;root&gt;/devices/&lt;id&gt;/&lt;value&gt;</code> and <code>&lt;root&gt;/alarm</code>,
						<code>alarming_count</code>, <code>fault_count</code>.
					</span>
				</div>
				<div class="grid w-full grid-cols-1 content-center gap-x-4 gap-y-2">
					<div class="flex items-center w-full mt-2">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('treeEnabled') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer justify-start gap-2 items-center">
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.treeEnabled} />
							<div class="flex items-center gap-2">
								<span>Enable generic value tree</span>
								<DirtyMarker dirty={f.isDirty('treeEnabled')} onrevert={() => f.revert('treeEnabled')} />
							</div>
						</label>
					</div>
					<div>
						<label class="label" for="treeBaseTopic">Root topic (empty = genius-gateway/&lt;mac&gt;)</label>
						<DirtyField dirty={f.isDirty('treeBaseTopic')} onrevert={() => f.revert('treeBaseTopic')}>
							<input
								type="text"
								placeholder="genius-gateway/<mac>"
								class="input w-full pr-10 {treeBaseTopicError ? 'border-error border-2' : ''}"
								bind:value={f.current.treeBaseTopic}
								id="treeBaseTopic"
								max={maxTopicPathLength}
							/>
						</DirtyField>
						<FieldError show={treeBaseTopicError} message="Topic must be at most {maxTopicPathLength} characters (a–z, A–Z, 0–9, -, _, ., /). No leading/trailing slashes." />
					</div>
				</div>
				<div class="divider mb-2 mt-0"></div>
			<div class="mx-4 flex flex-wrap justify-end gap-2">
				<button
					class="btn btn-primary"
					type="submit"
					disabled={!f.anyDirty || alarmTopicError || treeBaseTopicError}
					>Apply Settings</button
				>
			</div>
		</form>
	{/if}
</Collapsible>
