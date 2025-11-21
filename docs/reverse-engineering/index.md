# Reverse Engineering

!!! warning "Disclaimer"
    All results documented in this section do not claim to be factually accurate. They are observations and conclusions drawn from them and may contradict the actual implementation.
    
    Any use of this information is at your own risk and responsibility.

## Equipment

For the analysis, I used the following equipment:

* Microscope
* Digital oscilloscope
* Logic analyzer

## Components

The analysis primarily focused on the [FM Basis X :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/funkmodul-basis-x){ target=_blank } radio module.

## Results

The analysis was carried out in the following steps:

* Investigation of the [:octicons-arrow-right-24: PCB](fm-basis-rx-pcb.md)
* Determination of the [:octicons-arrow-right-24: RF configuration](fm-basis-x-rf.md)
* Investigation of the [:octicons-arrow-right-24: communication protocol](protocol-analysis.md)