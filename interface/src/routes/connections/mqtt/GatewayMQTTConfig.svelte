<script lang="ts">
	import { onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import IconSmarthome from '~icons/tabler/smart-home';
	import IconHomeAssistant from '~icons/custom-icons/home-assistant';
	import Info from '~icons/tabler/info-circle';
	import Collapsible from '$lib/components/Collapsible.svelte';

	type GatewayMQTTSettings = {
		haMQTTEnabled: boolean;
		haMQTTTopicPrefix: string;
		alarmEnabled: boolean;
		alarmTopic: string;
	};

	const maxTopicPathLength = 64;

	const defaultSettings: GatewayMQTTSettings = {
		haMQTTEnabled: false,
		haMQTTTopicPrefix: 'homeassistant/binary_sensor/genius-',
		alarmEnabled: false,
		alarmTopic: 'smarthome/genius-gateway/alarm'
	};

	let mqttSettings: GatewayMQTTSettings = $state(defaultSettings);
	let strSettings: string = $state(JSON.stringify(defaultSettings)); // to recognize changes
	let isSettingsDirty: boolean = $derived(JSON.stringify(mqttSettings) !== strSettings);

	function isValidMQTTTopicPath(topic: string): boolean {
		// MQTT topics must not be empty, must not contain wildcards (+ or #), and must not contain null character
		if (!topic || typeof topic !== 'string') return false;
		if (topic.length < 1 || topic.length > 128) return false;
		if (topic.includes('+') || topic.includes('#') || topic.includes('\u0000')) return false;
		// Topics must not start or end with a slash, but can contain slashes
		if (topic.startsWith('/') || topic.endsWith('/')) return false;
		// No empty topic levels (i.e., no double slashes)
		if (topic.includes('//')) return false;
		return true;
	}

	async function getGatewayMQTTSettings() {
		try {
			const response = await fetch('/rest/mqtt-settings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			mqttSettings = await response.json();
			strSettings = JSON.stringify(mqttSettings); // Store the recently loaded settings in a string variable
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getGatewayMQTTSettings();
		}
	});

	let formErrors = $state({
		haMQTTTopicPrefix: false,
		alarmTopic: false
	});

	let hasError = $derived(formErrors.haMQTTTopicPrefix || formErrors.alarmTopic);

	async function postGatewayMQTTSettings() {
		try {
			const response = await fetch('/rest/mqtt-settings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(mqttSettings)
			});
			if (response.status == 200) {
				notifications.success('MQTT settings updated.', 3000);
				mqttSettings = await response.json();
				strSettings = JSON.stringify(mqttSettings); // Store the recently loaded settings in a string variable
			} else {
				notifications.error('Updating MQTT settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}
</script>

<Collapsible open={false} class="shadow-lg" isDirty={isSettingsDirty}>
	{#snippet icon()}
		<IconSmarthome class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	{/snippet}
	{#snippet title()}
		<span>Smart Home Integration</span>
	{/snippet}
	<div class="grid w-full grid-cols-1 content-center gap-x-4 px-4">
		<div class="flex flex-col py-2">
			<label class="label cursor-pointer">
				<input
					type="checkbox"
					class="toggle toggle-primary"
					bind:checked={mqttSettings.alarmEnabled}
				/>
				<span class="ml-1">Enable simple alarm publishing</span>
			</label>
		</div>
		{#if mqttSettings.alarmEnabled}
			<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
				<label class="label" for="alarmTopic">
					<span class="label-text text-md">Alarm Topic</span>
				</label>
				<input
					type="text"
					placeholder={`E.g. ${defaultSettings.alarmTopic}`}
					class="input input-bordered w-full invalid:border-error invalid:border-2 {formErrors.alarmTopic
						? 'border-error border-2'
						: ''}"
					bind:value={mqttSettings.alarmTopic}
					id="alarmTopic"
					min="1"
					max={maxTopicPathLength}
					required
					disabled={!mqttSettings.alarmEnabled}
					oninput={(event: Event) => {
						formErrors.alarmTopic =
							!(event.target as HTMLInputElement).validity.valid ||
							!isValidMQTTTopicPath(mqttSettings.alarmTopic);
					}}
				/>
				{#if formErrors.alarmTopic}
					<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<label class="label" for="alarmTopic">
							<span class="label-text-alt text-error text-xs text-wrap"
								>MQTT topics must have valid syntax and 1 - {maxTopicPathLength} characters.</span
							>
						</label>
					</div>
				{/if}
				{#if !formErrors.alarmTopic && mqttSettings.alarmEnabled}
					<div
						class="alert bg-base-300 mt-1 mb-2"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<Info class="h-6 w-6 shrink-0" />
						<div>
							<div>
								As soon as a smoke detector is alarming, it will be published to this central topic.
							</div>
						</div>
					</div>
				{/if}
			</div>
		{/if}
		<div class="flex flex-col py-2">
			<label class="label cursor-pointer">
				<input
					type="checkbox"
					class="toggle toggle-primary"
					bind:checked={mqttSettings.haMQTTEnabled}
				/>
				<span class="ml-1">Enable device publishing</span>
			</label>
		</div>
		{#if mqttSettings.haMQTTEnabled}
			<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
				<label class="label" for="haPath">
					<span class="label-text text-md">MQTT Topics Path Prefix</span>
				</label>
				<input
					type="text"
					placeholder={`E.g. ${defaultSettings.haMQTTTopicPrefix}`}
					class="input input-bordered w-full invalid:border-error invalid:border-2 {formErrors.haMQTTTopicPrefix
						? 'border-error border-2'
						: ''}"
					bind:value={mqttSettings.haMQTTTopicPrefix}
					id="haPath"
					min="1"
					max={maxTopicPathLength}
					required
					disabled={!mqttSettings.haMQTTEnabled}
					oninput={(event: Event) => {
						formErrors.haMQTTTopicPrefix =
							!(event.target as HTMLInputElement).validity.valid ||
							!isValidMQTTTopicPath(mqttSettings.haMQTTTopicPrefix);
					}}
				/>
				{#if formErrors.haMQTTTopicPrefix}
					<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<label class="label" for="haPath">
							<span class="label-text-alt text-error text-xs text-wrap"
								>MQTT topics must have valid syntax and 1 - {maxTopicPathLength} characters.</span
							>
						</label>
					</div>
				{/if}
				{#if !formErrors.haMQTTTopicPrefix && mqttSettings.haMQTTEnabled}
					<div
						class="alert bg-base-300 mt-1 mb-2"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<IconHomeAssistant class="h-6 w-6 shrink-0" />
						<div>
							<div>
								Every smoke detector device will publish the following <a class="link link-primary" href="https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery">Home Assistant</a> compatible
								topics:
							</div>
							<div class="text-xs">
								<ul class="list-disc pl-4">
									<li>
										<code
											>{mqttSettings.haMQTTTopicPrefix}<span class="text-secondary"
												>&lt;Detector-Serialnumber&gt;</span
											>/config</code
										>
									</li>
									<li>
										<code
											>{mqttSettings.haMQTTTopicPrefix}<span class="text-secondary"
												>&lt;Detector-Serialnumber&gt;</span
											>/state</code
										>
									</li>
								</ul>
							</div>
						</div>
					</div>
				{/if}
			</div>
		{/if}
	</div>

	<div class="divider mb-2 mt-0"></div>
	<div class="mx-4 flex flex-wrap justify-end gap-2">
		<button
			class="btn btn-primary"
			disabled={hasError || !isSettingsDirty}
			onclick={postGatewayMQTTSettings}>Apply Settings</button
		>
	</div>
</Collapsible>
