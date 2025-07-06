import type { GeniusDevices, GeniusDevice } from '$lib/types/models';

export function createGeniusDevices() {

let geniusDevices: GeniusDevices = $state({ devices: [] } as GeniusDevices);
let isAlarming: boolean = $derived(geniusDevices.devices.some((device) => device.isAlarming));

return ({
    get devices() {
        return geniusDevices.devices;
    },
    set devices(newDevices: GeniusDevice[]) {
        geniusDevices.devices = newDevices;
    },
    get isAlarming() {
        return isAlarming;
    }
});
}

export const geniusDevices = createGeniusDevices()