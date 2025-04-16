import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'Fire Alarm'
    };
}) satisfies PageLoad;