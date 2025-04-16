import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'Gateway Settings'
    };
}) satisfies PageLoad;