/**
 * Utility functions for serializing and deserializing objects containing Uint8Array
 */

export interface SerializablePacket {
  [key: string]: any;
  data?: Uint8Array;
}

/**
 * Safely serialize an object containing Uint8Array to JSON string
 */
export function serializePacket(obj: SerializablePacket): string {
  return JSON.stringify(obj, (key, value) => {
    if (value instanceof Uint8Array) {
      return {
        __type: 'Uint8Array',
        data: Array.from(value)
      };
    }
    return value;
  }, 2);
}

/**
 * Safely deserialize a JSON string back to an object with Uint8Array
 */
export function deserializePacket(jsonString: string): SerializablePacket {
  return JSON.parse(jsonString, (key, value) => {
    if (value && typeof value === 'object' && value.__type === 'Uint8Array') {
      return new Uint8Array(value.data);
    }
    return value;
  });
}

/**
 * Downloads a Genius Packet as a JSON file
 */
export function downloadPacketAsJson(exportPacket: any, exportName: string){
    var dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(serializePacket(exportPacket));
    var downloadAnchorNode = document.createElement('a');
    downloadAnchorNode.setAttribute("href", dataStr);
    downloadAnchorNode.setAttribute("download", exportName);
    document.body.appendChild(downloadAnchorNode); // required for Firefox
    downloadAnchorNode.click();
    downloadAnchorNode.remove();
  }