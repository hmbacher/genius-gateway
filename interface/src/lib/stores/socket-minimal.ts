import { writable } from 'svelte/store';
import msgpack from 'msgpack-lite';

function createWebSocket() {
	let listeners = new Map<string, Set<(data?: unknown) => void>>();
	const { subscribe, set } = writable(false);
	const socketEvents = ['open', 'close', 'error', 'message', 'unresponsive'] as const;
	type SocketEvent = (typeof socketEvents)[number];
	let unresponsiveTimeoutId: ReturnType<typeof setTimeout>;
	let reconnectTimeoutId: ReturnType<typeof setTimeout>;
	let ws: WebSocket;
	let socketUrl: string | URL;
	let event_use_json = false;

	// Simple connection metrics for robustness
	let lastMessageAt = 0;

	function init(url: string | URL, use_json: boolean = false) {
		socketUrl = url;
		event_use_json = use_json;
		connect();
	}

	function connect() {
		if (ws?.readyState === WebSocket.OPEN) {
			ws.close();
		}

		try {
			ws = new WebSocket(socketUrl);
		} catch (error) {
			console.error('WebSocket connection failed:', error);
			scheduleReconnect();
			return;
		}

		ws.onopen = () => {
			set(true);
			resetUnresponsiveCheck();
			notifyListeners('open');
		};

		ws.onclose = (event) => {
			set(false);
			clearTimeout(unresponsiveTimeoutId);
			
			// Determine disconnect reason for better reconnection strategy
			let reason = 'close';
			if (event.code) {
				reason = `close:${event.code}`;
			}
			
			notifyListeners('close', { reason, code: event.code });
			scheduleReconnect();
		};

		ws.onerror = (event) => {
			console.error('WebSocket error:', event);
			notifyListeners('error', event);
			// Error will trigger onclose, so no need to reconnect here
		};

		ws.onmessage = (event) => {
			lastMessageAt = Date.now();
			resetUnresponsiveCheck(); // Key robustness feature: reset timeout on each message
			
			try {
				let data;
				if (event_use_json) {
					data = JSON.parse(event.data);
				} else {
					const uint8Array = new Uint8Array(event.data);
					data = msgpack.decode(uint8Array);
				}

				if (data?.event) {
					notifyListeners(data.event, data.data);
				}
			} catch (error) {
				console.error('Failed to parse message:', error);
			}
		};
	}

	// Key robustness improvement: proactive unresponsive detection
	function resetUnresponsiveCheck() {
		clearTimeout(unresponsiveTimeoutId);
		unresponsiveTimeoutId = setTimeout(() => {
			console.warn('WebSocket appears unresponsive, reconnecting...');
			disconnect('unresponsive');
			scheduleReconnect();
		}, 2000); // 2 second timeout
	}

	function disconnect(reason: SocketEvent, event?: Event) {
		set(false);
		clearTimeout(unresponsiveTimeoutId);
		clearTimeout(reconnectTimeoutId);
		
		if (ws && ws.readyState === WebSocket.OPEN) {
			ws.close();
		}
		
		notifyListeners('close', { reason });
	}

	// Improved reconnection with backoff
	function scheduleReconnect() {
		clearTimeout(reconnectTimeoutId);
		reconnectTimeoutId = setTimeout(() => {
			if (ws?.readyState !== WebSocket.OPEN) {
				console.log('Attempting WebSocket reconnection...');
				connect();
			}
		}, 1000); // 1 second delay
	}

	function notifyListeners(event: string, data?: any) {
		const eventListeners = listeners.get(event);
		if (eventListeners) {
			eventListeners.forEach(listener => {
				try {
					listener(data);
				} catch (error) {
					console.error(`Error in ${event} listener:`, error);
				}
			});
		}
	}

	function send(msg: unknown) {
		if (!ws || ws.readyState !== WebSocket.OPEN) {
			console.warn('WebSocket not open, cannot send message');
			return;
		}

		try {
			if (event_use_json) {
				ws.send(JSON.stringify(msg));
			} else {
				const packed = msgpack.encode(msg);
				ws.send(packed);
			}
		} catch (error) {
			console.error('Failed to send WebSocket message:', error);
		}
	}

	function sendEvent(event: string, data: unknown) {
		send({ event, data });
	}

	function unsubscribe(event: string, listener?: (data: any) => void) {
		let eventListeners = listeners.get(event);
		if (!eventListeners) return;

		if (listener) {
			eventListeners.delete(listener);
		} else {
			eventListeners.clear();
		}

		// If no more listeners for this event, unsubscribe from server
		if (eventListeners.size === 0) {
			if (!socketEvents.includes(event as SocketEvent)) {
				sendEvent('unsubscribe', event);
			}
			listeners.delete(event);
		}
	}

	return {
		subscribe,
		send,
		sendEvent,
		init,
		on: <T>(event: string, listener: (data: T) => void): (() => void) => {
			let eventListeners = listeners.get(event);
			if (!eventListeners) {
				if (!socketEvents.includes(event as SocketEvent)) {
					sendEvent('subscribe', event);
				}
				eventListeners = new Set();
				listeners.set(event, eventListeners);
			}
			eventListeners.add(listener as (data: any) => void);

			return () => {
				unsubscribe(event, listener);
			};
		},
		off: (event: string, listener?: (data: any) => void) => {
			unsubscribe(event, listener);
		}
	};
}

export const socket = createWebSocket();