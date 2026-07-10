<script lang="ts">
	import type { PageData } from './$types';
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import type { userProfile } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import InputPassword from '$lib/components/InputPassword.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import EditUser from './EditUser.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import Delete from '~icons/tabler/trash';
	import AddUser from '~icons/tabler/user-plus';
	import Edit from '~icons/tabler/pencil';
	import Admin from '~icons/tabler/key';
	import Users from '~icons/tabler/users';
	import Warning from '~icons/tabler/alert-triangle';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	type userSetting = {
		username: string;
		password: string;
		admin: boolean;
	};

	type SecuritySettings = {
		jwt_secret: string;
		users: userSetting[];
	};

	// jwt_secret is the only form-edited field; users are managed write-through via modals.
	const f = createDirtyState<{ jwt_secret: string }>({ jwt_secret: '' });
	let users = $state<userSetting[]>([]);

	async function getSecuritySettings() {
		try {
			const response = await fetch('/rest/securitySettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			const data: SecuritySettings = await response.json();
			f.reset({ jwt_secret: data.jwt_secret });
			users = data.users;
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postSecuritySettings() {
		try {
			const response = await fetch('/rest/securitySettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({ jwt_secret: f.current.jwt_secret, users })
			});

			const result: SecuritySettings = await response.json();
			users = result.users;
			f.reset({ jwt_secret: result.jwt_secret });
			if (response.status == 200) {
				if (await validateUser($user)) {
					notifications.success('Security settings updated.', 3000);
				}
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function validateUser(userdata: userProfile) {
		try {
			const response = await fetch('/rest/verifyAuthorization', {
				method: 'GET',
				headers: {
					Authorization: 'Bearer ' + userdata.bearer_token,
					'Content-Type': 'application/json'
				}
			});
			if (response.status !== 200) {
				user.invalidate();
				return false;
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return true;
	}

	function confirmDelete(index: number) {
		modals.open(ConfirmDialog, {
			title: 'Confirm Delete User',
			message: 'Are you sure you want to delete the user "' + users[index].username + '"?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Yes', icon: Check }
			},
			onConfirm: () => {
				users.splice(index, 1);
				modals.close();
				postSecuritySettings();
			}
		});
	}

	function handleEdit(index: number) {
		modals.open(EditUser, {
			title: 'Edit User',
			titleIcon: Edit,
			user: { ...users[index] },
			onSaveUser: (editedUser: userSetting) => {
				users[index] = editedUser;
				modals.close();
				postSecuritySettings();
			}
		});
	}

	function handleNewUser() {
		modals.open(EditUser, {
			title: 'Add User',
			titleIcon: AddUser,
			onSaveUser: (newUser: userSetting) => {
				users = [...users, newUser];
				modals.close();
				postSecuritySettings();
			}
		});
	}
</script>

{#if $user.admin}
	<div
		class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
	>
		<SettingsCard collapsible={false} isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
			{#snippet icon()}
				<Users class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>Manage Users</span>
			{/snippet}
			{#await getSecuritySettings()}
				<Spinner />
			{:then nothing}
				<div class="relative w-full overflow-visible">
					<button
						class="btn btn-primary text-primary-content btn-md absolute -top-14 right-0"
						onclick={handleNewUser}
					>
						<AddUser class="h-6 w-6" /></button
					>

					<div class="overflow-x-auto" transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<table class="table w-full table-auto">
							<thead>
								<tr class="font-bold">
									<th align="left">Username</th>
									<th align="center">Admin</th>
									<th align="right" class="pr-8">Edit</th>
								</tr>
							</thead>
							<tbody>
								{#each users as u, index}
									<tr>
										<td align="left">{u.username}</td>
										<td align="center">
											{#if u.admin}
												<Admin class="text-secondary" />
											{/if}
										</td>
										<td align="right">
											<span class="my-auto inline-flex flex-row space-x-2">
												<button
													class="btn btn-ghost btn-circle btn-xs"
													onclick={() => handleEdit(index)}
												>
													<Edit class="h-6 w-6" /></button
												>
												<button
													class="btn btn-ghost btn-circle btn-xs"
													onclick={() => confirmDelete(index)}
												>
													<Delete class="text-error h-6 w-6" />
												</button>
											</span>
										</td>
									</tr>
								{/each}
							</tbody>
						</table>
					</div>
				</div>
				<div class="divider mb-0"></div>

				<span class="pb-2 text-xl font-medium">Security Settings</span>
				<div class="alert alert-warning shadow-lg">
					<Warning class="h-6 w-6 shrink-0" />
					<span
						>The JWT secret is used to sign authentication tokens. If you modify the JWT Secret, all
						users will be signed out.</span
					>
				</div>
				<label class="label" for="secret">JWT Secret</label>
				<InputPassword
					bind:value={f.current.jwt_secret}
					id="secret"
					baseline={f.baseline.jwt_secret}
					onrevert={() => f.revert('jwt_secret')}
				/>
				<div class="mt-6 flex justify-end">
					<button class="btn btn-primary" disabled={!f.anyDirty} onclick={postSecuritySettings}
						>Apply Settings</button
					>
				</div>
			{/await}
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
