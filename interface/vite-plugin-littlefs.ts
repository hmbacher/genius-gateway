import type { UserConfig, Plugin } from 'vite';

export default function viteLittleFS(): Plugin[] {
	return [
		{
			name: 'vite-plugin-littlefs',
			enforce: 'post',
			apply: 'build',

			async config(config, _configEnv) {
				// When the firmware embeds assets into PROGMEM (EMBED_WWW), there
				// is no 32-char filename limit and Rollup's content hashes must be
				// preserved - without them, /_app/immutable/* filenames are stable
				// across firmware builds while their contents change, which makes
				// the "Cache-Control: immutable, max-age=31536000" header a lie and
				// pins stale bundles in browsers forever.
				if (process.env.EMBED_WWW) return;

				const rollupOptions = config.build?.rollupOptions;
				const output = rollupOptions?.output;
				if (!rollupOptions || !output || Array.isArray(output)) return;

				const { assetFileNames, chunkFileNames, entryFileNames } = output;
				const next = { ...output };

				// Handle Server-build + Client Assets
				if (typeof assetFileNames === 'string') {
					next.assetFileNames = assetFileNames.replace('.[hash]', '');
				}

				// Handle Client-build
				if (typeof chunkFileNames === 'string' && chunkFileNames.includes('hash')) {
					next.chunkFileNames = chunkFileNames.replace('.[hash]', '');
					if (typeof entryFileNames === 'string') {
						next.entryFileNames = entryFileNames.replace('.[hash]', '');
					}
				}

				rollupOptions.output = next;
      }
    }
  ]
}
