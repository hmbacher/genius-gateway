import type { PageLoad } from './$types';

export const load = (async () => {
    return {
        title: 'Smoke detectors'
    };
}) satisfies PageLoad;