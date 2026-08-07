# hello_mics

On-hardware smoke test for the four-channel onboard PDM microphone driver.
The LCD shows live meters in physical left-to-right order **D, B, A, C**;
RTT carries exact RMS and peak values about three times per second.

    fw build hello_mics
    fw flash hello_mics
    fw rtt

All channels should show low ambient activity. Tapping or speaking near each
microphone should spike its corresponding meter. A channel stuck at zero or
frozen indicates a microphone-power, pad-isolation, PIO, or DMA regression.
