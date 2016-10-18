# DMA1

* Divide the screen into three tiles.
* Come up with a suitable test pattern.  Maybe a rotating wheel.

# Peripherals

We use one timer, as described in AN4666 Figure 1.  It drives one
DMA channel, and the DMA channel writes to one GPIO port.

## GPIO

See Piotr's drawing.

## Timer

The LCD_WR pin is pin PB1.  It has alternate functions T1C3N, T3C4,
and T8C3N.  All three of those are 16 bits and can generate DMA
requests.  Timers 1 and 8 have a max speed of 168 MHz.  Timer 3's
max speed is 84 MHz.  84 MHz is fine.

But Timer 3 can only trigger DMA from DMA1.  DMA1 can't access
AHB1 registers.  Tbe GPIO register we want is in AHB1 space.  So
we have to use TIM1 or TIM8.

The LCD_RD pin is PB0.  It has similar alternate functions T1C2N,
T3C3, T8C2N.  So, if we later implement read DMA, the same
considerations apply.

So I'll use Timer 8.

Initially I will run the timer at 5 MHz, 50% duty
cycle.  (Data setup time: 100 nsec).  The ILI9341 datasheets says
it will run at 15.151515 MHz with a minimum data setup time of 10
nsec.

See the Technical Reference, Document RM0090, Table 42, "DMA1 Request
Mapping".

> 18.3.7 Forced output Mode
>
> In output mode (CCMRx.CCxS bits = 00), each output compare signal
> can be forced to active or inactive level directly by software,
> independently of any comparison between the output compare register
> and the counter.

So perhaps there's no need to switch the LCD_WRX and LCD_RDX pins
in and out of Alternate Function mode.

I want PWM Mode (18.3.9).  PWM mode 2: OCxREF is active when CNT > CCR1.


  - Timer: 8
  - Write comparator: T8C3
  - Read comparator: T8C2 (future)
  - Mode: count up
  - overflow value: 34 (initially)
  - comparator value: 17 (initially)
  - trigger DMA: on overflow
  - DMA controller: DMA2
  - DMA stream: 1
  - DMA channel: 7 (TIM8\_UP).
  - TIM8_ARR = 168 MHz / 5 MHz - 1
  - ARPE = don't care
  - UDIS = don't care
  - upcounting mode
  - SMCR.SMS = CLK_INT
  - TIx?
  - CCMRx.OCxM = PWM mode 1
  - DBGMCU.DBG_TIM3_STOP = Yes
  

### One-pulse mode

It looks intriguing.  It means we wouldn't have to worry about overflowing the
buffer.  (That's especially important when reading.)

It looks like Timer A would run in PWM mode as normal and trigger
timer B on every overflow.  Timer B would run in one-pulse mode and on
counting the right number of triggers, it would generate a single DMA to
write Timer A's register and stop it.  And raise an interrupt.




## DMA for video I/O

Because the data transfer may overrun, the DMA needs to be configured
for a circular buffer.  And it needs to interrupt when it reaches the end.

  - Mode: Memory to Peripheral.
  - Source Address: start of frame
  - Destination Address: GPIOB_ODR + 1 (0x40020414)
  - MSIZE: 4
  - PSIZE: 1
  - MINC: Yes
  - PINC: No
  - MBURST: 4 (limited by FIFO size)
  - PBURST: 1
  - PINCOS: 0 (I think)
  - Circular: Yes
  - Double Buffer: No
  - SxNDTR: 240 * H * 2
  - DMDIS: Yes (do disable Direct Mode)
  - PFCTRL (Flow Control): DMA
  - TCIE: Yes
  - TEIE: Yes (transfer error interrupt enable)
  - FEIE: Yes (fifo error interrupt enable)
  - DMEIF: Yes (direct mode error interrupt enable)

N.B., DMA bursts have to be aligned to 1 KB boundary.  See RM0090 Section
10.3.11, last paragraph.  I think that is not an issue -- a scan line
is a multiple of the burst size, so as long as the buffer is full scan
lines, no bursts will be misaligned.

If I understand correctly, Burst mode uses less of the available AHB
capacity, but it increases the likelihood of underflow and slightly
increases interrupt latency.  When pushing the clock speed higher, it
might make sense to disable burst mode.

Interrupt on transfer complete.

Priority?

## DMA for memory clear

We can use the other DMA controller to clear memory.  Presumably that
will be less overhead than using the CPU.

In memory-to-memory mode, the peripheral port is the source, and the
memory port is the destination.

We can clear 32 bits at a time if we load a uint32_t with two copies
of the pixel.

Don't want to block video I/O.  So use small burst size and low
priority.  Or just have a compile-time flag for bursting.


  - DMA controller: DMA2
  - Mode: memory to memory
  - source address: address of a constant pixel
  - destination address: start of frame
  - PSIZE: 4
  - PINC: No
  - PBURST: 4
  - MSIZE: 4
  - MINC: Yes
  - MBURST: 4
  - Circular: No
  - Double Buffer: No
  - SxNDTR: 240 * H * 2 / 4
  - PFCTRL: No
  - DMDIS: No
  - PFCTRL (Flow Control): DMA
  - TCIE: Yes
  - TEIE: Yes
  - FEIE: Yes
  - DMEIF: Yes

# State Machines

## Buffer

A buffer has five states.  It goes through them in sequence.

  - clear, idle
  - drawing
  - waiting to send
  - sending
  - waiting to clear
  - clearing
  
## Video DMA

The Video DMA engine has states.

  - idle
  - sending
  - broken
  
## Clear DMA
  
The Clear DMA engine has states.

  - idle
  - clearing
  - broken

## Main Loop

The main loop is:

    for tile in 0..3:
        allocate tile
        draw tile
        send tile to screen.

`allocate` and `send` block until the DMA engine is ready.

# Pseudocode

## Init 

## allocate tile

    while buffer state != idle:
        wait
    ATOMIC {
        buffer state = drawing
    }

## send tile

    assert buffer state == drawing
    buffer state = waiting to send
    while video DMA state != idle:
        wait
    if buffer number == 0 and VSYNC enabled:
        wait for VSYNC
    init DMA
    init timer
    buffer state = sending
    start timer
    
## video DMA interrupt

    stop timer
    check for errors
    buffer state = clearing
    find and start next buffer, if any.
