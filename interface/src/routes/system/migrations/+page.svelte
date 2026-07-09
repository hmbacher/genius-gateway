<script lang="ts">
	import { onMount } from 'svelte';
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import Migrate from '~icons/tabler/database-import';
	import Info from '~icons/tabler/info-circle';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import CircleCheck from '~icons/tabler/circle-check';
	import CircleX from '~icons/tabler/circle-x';
	import Clock from '~icons/tabler/clock';
	import CircleMinus from '~icons/tabler/circle-minus';

	type State = 'failed' | 'pending' | 'applied' | 'notApplicable';

	type MigrationEntry = {
		id: string;
		state: State;
		phase?: 'pre' | 'post';
		order?: number;
		onFailure?: 'abortBoot' | 'retryNextBoot' | 'skipAfterRetries';
		maxAttempts?: number;
		appliedAt?: string;
		attempts?: number;
		lastError?: string;
		registered?: boolean;
	};

	type Response = { migrations: MigrationEntry[] };

	type Row = {
		id: string;
		state: State;
		notes: string;
		critical: boolean;
	};

	let data = $state<Response | null>(null);
	let loading = $state(true);
	let error = $state<string | null>(null);
	let retryStatus = $state<string | null>(null);
	let retrying = $state(false);

	function authHeader(): string {
		return page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic';
	}

	async function load() {
		loading = true;
		error = null;
		try {
			const r = await fetch('/rest/migrations', {
				headers: { Authorization: authHeader() }
			});
			if (!r.ok) throw new Error('HTTP ' + r.status);
			data = await r.json();
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to load migrations';
		} finally {
			loading = false;
		}
	}

	async function retry() {
		retrying = true;
		retryStatus = null;
		try {
			const r = await fetch('/rest/migrations/retry', {
				method: 'POST',
				headers: { Authorization: authHeader() }
			});
			if (!r.ok) throw new Error('HTTP ' + r.status);
			retryStatus = 'Failure records cleared. Reboot the device to retry.';
			await load();
		} catch (e) {
			retryStatus = 'Retry failed: ' + (e instanceof Error ? e.message : 'unknown error');
		} finally {
			retrying = false;
		}
	}

	onMount(load);

	const STATE_RANK: Record<State, number> = {
		failed: 0,
		pending: 1,
		applied: 2,
		notApplicable: 3
	};

	function notesFor(m: MigrationEntry): string {
		let base: string;
		switch (m.state) {
			case 'applied':
				base = m.appliedAt ? `Applied in ${m.appliedAt}` : 'Applied';
				break;
			case 'failed':
				base = `Attempt ${m.attempts ?? '?'}${m.lastError ? ' - ' + m.lastError : ''}`;
				break;
			case 'pending':
				base = `${m.phase ?? '?'} · order ${m.order ?? '?'} · ${m.onFailure ?? '?'}`;
				break;
			case 'notApplicable':
				base = 'Preconditions not met for this device';
				break;
		}
		if (m.registered === false) base += ' · no longer registered';
		return base;
	}

	function buildRows(d: Response): Row[] {
		return d.migrations
			.map((m) => ({
				id: m.id,
				state: m.state,
				notes: notesFor(m),
				critical: m.state === 'pending' && m.onFailure === 'abortBoot'
			}))
			.sort((x, y) => {
				const r = STATE_RANK[x.state] - STATE_RANK[y.state];
				if (r !== 0) return r;
				return x.id.localeCompare(y.id);
			});
	}
</script>

<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
	<SettingsCard collapsible={false}>
		{#snippet icon()}
			<Migrate class="h-6 w-6 rounded-full" />
		{/snippet}
		{#snippet title()}
			<span>Migrations</span>
		{/snippet}

		<div class="alert alert-info shadow-lg">
			<Info class="h-6 w-6 shrink-0" />
			<span>
				One-shot config transforms that run automatically across firmware upgrades. Applied
				migrations never re-run; not-applicable migrations have preconditions that aren't met on
				this device (e.g. no legacy config file exists).
			</span>
		</div>

		{#if loading}
			<div class="mt-4 flex justify-center">
				<Spinner />
			</div>
		{:else if error}
			<p class="text-error mt-4">{error}</p>
		{:else if data}
			{@const rows = buildRows(data)}
			{@const failedCount = rows.filter((r) => r.state === 'failed').length}

			{#if failedCount > 0}
				<div class="alert alert-warning mt-4 shadow-lg">
					<AlertTriangle class="h-6 w-6 shrink-0" />
					<span>
						{failedCount} failed migration{failedCount > 1 ? 's' : ''}.
					</span>
					{#if $user.admin}
						<button class="btn btn-warning btn-sm" disabled={retrying} onclick={retry}>
							{retrying ? 'Clearing…' : 'Retry on next reboot'}
						</button>
					{/if}
				</div>
				{#if retryStatus}
					<p class="text-info mt-2">{retryStatus}</p>
				{/if}
			{/if}

			{#if rows.length === 0}
				<p class="text-base-content/60 mt-4">No migrations registered.</p>
			{:else}
				<div class="mt-4 overflow-x-auto">
					<table class="table table-zebra table-sm w-full">
						<thead>
							<tr>
								<th>ID</th>
								<th>State</th>
								<th>Notes</th>
							</tr>
						</thead>
						<tbody>
							{#each rows as r (r.id)}
								<tr>
									<td><code>{r.id}</code></td>
									<td>
										{#if r.state === 'failed'}
											<span class="text-error inline-flex items-center gap-1 font-medium">
												<CircleX class="h-4 w-4 shrink-0" /> Failed
											</span>
										{:else if r.state === 'pending'}
											<span class="text-warning inline-flex items-center gap-1 font-medium">
												<Clock class="h-4 w-4 shrink-0" /> Pending
											</span>
											{#if r.critical}
												<span class="badge badge-error badge-sm ml-1">critical</span>
											{/if}
										{:else if r.state === 'notApplicable'}
											<span class="text-base-content/60 inline-flex items-center gap-1">
												<CircleMinus class="h-4 w-4 shrink-0" /> Not applicable
											</span>
										{:else}
											<span class="text-success inline-flex items-center gap-1 font-medium">
												<CircleCheck class="h-4 w-4 shrink-0" /> Applied
											</span>
										{/if}
									</td>
									<td>{r.notes}</td>
								</tr>
							{/each}
						</tbody>
					</table>
				</div>
			{/if}
		{/if}
	</SettingsCard>
</div>
