<script lang="ts">
	import { onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import IconAlarm from '~icons/tabler/bell';
	import Info from '~icons/tabler/info-circle';
	import Collapsible from '$lib/components/Collapsible.svelte';

	type AlarmPublishingSettings = {
		alarmEnabled: boolean;
		alarmTopic: string;
	};

	const maxTopicPathLength = 64;

	const defaultSettings: AlarmPublishingSettings = {
		alarmEnabled: false,
		alarmTopic: 'smarthome/genius-gateway/alarm'
	};

	let settings: AlarmPublishingSettings = $state(defaultSettings);
	let strSettings: string = $state(JSON.stringify(defaultSettings));
	let isSettingsDirty: boolean = $derived(JSON.stringify(settings) !== strSettings);

	function isValidMQTTTopicPath(topic: string): boolean {
		if (!topic || typeof topic !== 'string') return false;
		if (topic.length < 1 || topic.length > 128) return false;
		const validCharPattern = /^[a-zA-Z0-9\-_.\/]+$/;
		if (!validCharPattern.test(topic)) return false;
		if (topic.includes('+') || topic.includes('#') || topic.includes(' ')) return false;
		if (topic.startsWith('/') || topic.endsWith('/')) return false;
		if (topic.includes('//')) return false;
		const levels = topic.split('/');
		for (const level of levels) {
			if (level.length === 0 || level.trim().length === 0) return false;
		}
		return true;
	}

	async function getAlarmPublishingSettings() {
		try {
			const response = await fetch('/rest/alarm-publishing', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			settings = await response.json();
			strSettings = JSON.stringify(settings);
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

	const alarmTopicError = $derived(!isValidMQTTTopicPath(settings.alarmTopic));

	function handleSubmit() {
		if (!alarmTopicError) {
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
				body: JSON.stringify(settings)
			});
			if (response.status == 200) {
				notifications.success('Alarm settings updated.', 3000);
				settings = await response.json();
				strSettings = JSON.stringify(settings);
			} else {
				notifications.error('Updating alarm settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}
</script>

<Collapsible open={false} class="shadow-lg" isDirty={isSettingsDirty}>
	{#snippet icon()}
		<IconAlarm class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>Simple Alarm Publishing</span>
	{/snippet}
	<div class="alert alert-info my-2 shadow-lg">
		<Info class="h-6 w-6 shrink-0 stroke-current" />
		<span>As soon as a smoke detector is alarming, it will be published to a central MQTT topic.</span>
	</div>
	<div class="grid w-full grid-cols-1 content-center gap-x-4 px-4">
		<label class="label cursor-pointer justify-start gap-4">
			<input
				type="checkbox"
				class="toggle toggle-primary"
				bind:checked={settings.alarmEnabled}
			/>
			<span>Enable simple alarm publishing</span>
		</label>
		{#if settings.alarmEnabled}
			<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
				<label class="label" for="alarmTopic">Alarm Topic</label>
				<input
					type="text"
					placeholder={`E.g. ${defaultSettings.alarmTopic}`}
					class="input w-full invalid:border-error invalid:border-2 {alarmTopicError
						? 'border-error border-2'
						: ''}"
					bind:value={settings.alarmTopic}
					id="alarmTopic"
					min="1"
					max={maxTopicPathLength}
					required
					disabled={!settings.alarmEnabled}
				/>
				{#if alarmTopicError}
					<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<span class="text-error text-xs"
							>Topic must be 1–{maxTopicPathLength} characters (a–z, A–Z, 0–9, -, _, ., /). No
							leading/trailing slashes.</span
						>
					</div>
				{/if}
			</div>
		{/if}
	</div>

	<div class="divider mb-2 mt-0"></div>
	<div class="mx-4 flex flex-wrap justify-end gap-2">
		<button
			class="btn btn-primary"
			disabled={!isSettingsDirty || (settings.alarmEnabled && alarmTopicError)}
			onclick={handleSubmit}>Apply Settings</button
		>
	</div>
</Collapsible>
