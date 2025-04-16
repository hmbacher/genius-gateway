import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'Genius Smoke Detectors'
    };
}) satisfies PageLoad;