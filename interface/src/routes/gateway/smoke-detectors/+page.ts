import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'Smoke Detectors'
    };
}) satisfies PageLoad;