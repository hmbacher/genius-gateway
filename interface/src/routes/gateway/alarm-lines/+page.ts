import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'Alarm Lines'
    };
}) satisfies PageLoad;