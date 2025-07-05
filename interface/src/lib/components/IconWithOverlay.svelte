<script lang="ts">
	interface Props {
		baseIcon: any;
		overlayIcon: any;
		class?: string;
		size?: string;
		baseClass?: string;
		overlayClass?: string;
		overlayPosition?: 'center' | 'top-right' | 'bottom-right' | 'top-left' | 'bottom-left';
	}

	let {
		baseIcon: BaseIcon,
		overlayIcon: OverlayIcon,
		class: className = '',
		baseClass = '',
		overlayClass = '',
		overlayPosition = 'center'
	}: Props = $props();

	const positionClasses = {
		center: 'top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2',
		'top-right': 'top-0 right-0',
		'bottom-right': 'bottom-0 right-0',
		'top-left': 'top-0 left-0',
		'bottom-left': 'bottom-0 left-0'
	};
</script>

<div class="relative inline-block {className}">
	<BaseIcon class={baseClass} />
	<OverlayIcon
		class="absolute {positionClasses[overlayPosition]} {overlayClass}"
		style="pointer-events: none;"
	/>
</div>
