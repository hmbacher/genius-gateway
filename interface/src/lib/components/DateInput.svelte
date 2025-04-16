<script>
    import { run } from 'svelte/legacy';
  
    import { onMount } from 'svelte'
    import dayjs from 'dayjs'
  
    let { format = 'YYYY-MM-DD', date = $bindable(new Date()), id } = $props();
      
    let internal = $state()
  
    const input = (x) => (internal = dayjs(x).format(format))
    const output = (x) => (date = dayjs(x, format).toDate())
  
    run(() => {
      input(date)
    });
    run(() => {
      output(internal)
    });
  </script>
  
  <input type="date" bind:value={internal} class="input input-bordered invalid:border-error w-full invalid:border-2" id={id}/>
  