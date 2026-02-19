import type { GeniusDevices, GeniusDevice } from '$lib/types/models';

export function createGeniusDevices() {

let geniusDevices: GeniusDevices = $state({ devices: [] } as GeniusDevices);
let isAlarming: boolean = $derived(geniusDevices.devices.some((device) => device.isAlarming));
let isLoaded: boolean = $state(false);

return ({
    get devices() {
        return geniusDevices.devices;
    },
    set devices(newDevices: GeniusDevice[]) {
        geniusDevices.devices = newDevices;
        isLoaded = true;
    },
    get isAlarming() {
        return isAlarming;
    },
    get isLoaded() {
        return isLoaded;
    }
});
}

export const geniusDevices = createGeniusDevices()