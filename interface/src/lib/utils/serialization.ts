/**
 * Utility functions for serializing and deserializing objects containing Uint8Array
 */

import { downloadObjectAsJson } from "./misc";

export interface SerializablePacket {
  [key: string]: any;
  data?: Uint8Array;
}

/**
 * Safely serialize an object containing Uint8Array to JSON string
 */
export function packetSerializer(this: any, key: string, value: any): any {
  if (value instanceof Uint8Array) {
    return {
      __type: 'Uint8Array',
      data: Array.from(value)
    };
  }
  return value;
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
export function downloadPacketAsJson(exportPacket: any, exportName: string) {
  downloadObjectAsJson(exportPacket, exportName, true, packetSerializer);
}