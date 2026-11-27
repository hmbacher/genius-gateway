/**
 * Short description of a client from its User-Agent: "Chrome on Windows" for a browser, the first
 * product token for anything else ("Python/3.13 websockets/13.1" -> "Python/3.13 websockets/13.1").
 */
export function shortUserAgent(ua?: string): string {
	if (!ua) return '';
	const browsers: [RegExp, string][] = [
		[/\bEdg(e|A|iOS)?\//, 'Edge'],
		[/\bOPR\//, 'Opera'],
		[/\bFirefox\/|\bFxiOS\//, 'Firefox'],
		[/\bChrome\/|\bCriOS\//, 'Chrome'],
		[/\bVersion\/[\d.]+.*Safari\//, 'Safari']
	];
	const systems: [RegExp, string][] = [
		[/Android/, 'Android'],
		[/iPhone|iPad|iPod/, 'iOS'],
		[/Windows/, 'Windows'],
		[/Mac OS X|Macintosh/, 'macOS'],
		[/CrOS/, 'ChromeOS'],
		[/Linux/, 'Linux']
	];
	const browser = browsers.find(([re]) => re.test(ua))?.[1];
	if (browser) {
		const system = systems.find(([re]) => re.test(ua))?.[1];
		return system ? `${browser} on ${system}` : browser;
	}
	// Not a browser: tools name themselves at the start ("Python/3.13 websockets/13.1", "curl/8.5")
	return ua.split(/\s+\(/)[0].slice(0, 60);
}
