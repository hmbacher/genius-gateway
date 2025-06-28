import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'WebSocket Logger'
    };
}) satisfies PageLoad;