<script lang="ts">
	import { goto } from '$app/navigation';
	import { user } from '$lib/stores/user';
	import Info from '~icons/tabler/info-circle';
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
		<div class="alert alert-info">
			<Info class="h-6 w-6 shrink-0 stroke-current" />
			<div>This planned feature is not yet available.</div>
		</div>
	</div>
{:else}
	{goto('/')}
{/if}
