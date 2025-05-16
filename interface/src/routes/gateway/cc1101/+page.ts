import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'CC1101',
    };
}) satisfies PageLoad;