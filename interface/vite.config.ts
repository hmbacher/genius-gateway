import { sveltekit } from '@sveltejs/kit/vite';
import { createLogger } from 'vite';
import type { Logger, UserConfig } from 'vite';
import Icons from 'unplugin-icons/vite';
import viteLittleFS from './vite-plugin-littlefs';
import viteAppVersion from './vite-plugin-app-version';
import { FileSystemIconLoader } from 'unplugin-icons/loaders'
import tailwindcss from '@tailwindcss/vite'

/**
 * Dev server only: a browser that drops its connection while the proxy writes to it - a reload, a
 * phone that switches networks or apps - makes the proxy log an error with a stack trace. These codes
 * are expected and get one short line instead; every other error is logged as before.
 */
const CLOSED_BY_BROWSER = new Set(['ECONNABORTED', 'ECONNRESET', 'EPIPE']);

function proxyQuietLogger(): Logger {
	const logger = createLogger();
	const error = logger.error.bind(logger);
	logger.error = (msg, options) => {
		const code = (options?.error as { code?: string } | undefined)?.code;
		if (code && CLOSED_BY_BROWSER.has(code) && msg.includes('proxy')) {
			logger.warn(`proxy: connection closed by the browser (${code})`, { timestamp: true });
			return;
		}
		error(msg, options);
	};
	return logger;
}

const config: UserConfig = {
	customLogger: proxyQuietLogger(),
	plugins: [
		sveltekit(),
		tailwindcss(),
		Icons({
			compiler: 'svelte',
			customCollections: {
				'custom-icons': FileSystemIconLoader('./src/lib/assets/icons'),
				'my-icons': {
					test: '<svg  xmlns="http://www.w3.org/2000/svg"  width="24"  height="24"  viewBox="0 0 24 24"  fill="none"  stroke="currentColor"  stroke-width="2"  stroke-linecap="round"  stroke-linejoin="round"  class="icon icon-tabler icons-tabler-outline icon-tabler-circle-rectangle"><path stroke="none" d="M0 0h24v24H0z" fill="none"/><path d="M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0 -18 0" /><path d="M7 10h10v4h-10z" /></svg>'
				}
		  	}
		}),
		// Shorten file names for LittleFS 32 char limit
		viteLittleFS(),
		// Stamp APP_VERSION_FULL into version.ts (frontend) and AppVersion.h (firmware)
		viteAppVersion()
	],
	server: {
		allowedHosts: ['gg-dev.bacher.cloud'],
		proxy: {
			// Proxying REST: http://localhost:5173/rest/bar -> http://192.168.1.83/rest/bar
			'/rest': {
				target: 'http://192.168.178.122',
				changeOrigin: true
			},
			// Proxying websockets ws://localhost:5173/ws -> ws://192.168.1.83/ws
			'/ws': {
				target: 'http://192.168.178.122',
				changeOrigin: true,
				ws: true
			}
		}
	},
	build: {
		minify: 'terser',
		sourcemap: false,
		rollupOptions: {
			output: {}
		},
		cssCodeSplit: true
	}
};

export default config;
