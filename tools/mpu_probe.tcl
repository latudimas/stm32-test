# Diagnostic only: bit-bang PB6/PB7 through SWD while the CPU is halted.
# No flash writes. Slow debug transactions supply all I2C timing delays.
# Run: openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
#              -f tools/mpu_probe.tcl
# The target is reset before/after the probe; animation pauses during it.
# Not a 400 kHz signal-integrity test or a production sensor driver.

proc line {pin high} {
    # RM0008 section 9.2.5 GPIOB_BSRR: release OD output or pull it low.
    if {$high} { set bit $pin } else { set bit [expr {$pin + 16}] }
    mww 0x40010c10 [expr {1 << $bit}]
}
proc level {pin} {
    # RM0008 section 9.2.3 GPIOB_IDR: actual pin level.
    return [expr {([lindex [read_memory 0x40010c08 32 1] 0] >> $pin) & 1}]
}
proc scl_high {} {
    line 6 1
    for {set i 0} {$i < 10} {incr i} {
        if {[level 6]} { return }
    }
    error "SCL stuck low / excessive clock stretching"
}
proc start {} {
    line 6 0
    line 7 1
    scl_high
    if {![level 7]} { error "SDA stuck low before START" }
    line 7 0
    line 6 0
}
proc stop {} {
    line 6 0
    line 7 0
    scl_high
    line 7 1
}
proc tx {value} {
    for {set i 7} {$i >= 0} {incr i -1} {
        line 7 [expr {($value >> $i) & 1}]
        scl_high
        line 6 0
    }
    line 7 1
    scl_high
    set ack [expr {![level 7]}]
    line 6 0
    return $ack
}
proc rx {ack} {
    line 7 1
    set value 0
    for {set i 0} {$i < 8} {incr i} {
        scl_high
        set value [expr {($value << 1) | [level 7]}]
        line 6 0
    }
    line 7 [expr {!$ack}]
    scl_high
    line 6 0
    line 7 1
    return $value
}
proc probe {addr} {
    start
    set ack [tx [expr {$addr << 1}]]
    stop
    return $ack
}
proc read_regs {addr reg count} {
    start
    if {![tx [expr {$addr << 1}]] || ![tx $reg]} { error "Register pointer NACK" }
    start
    if {![tx [expr {($addr << 1) | 1}]]} { error "Read address NACK" }
    set bytes {}
    for {set i 0} {$i < $count} {incr i} {
        lappend bytes [rx [expr {$i + 1 < $count}]]
    }
    stop
    return $bytes
}
proc write_reg {addr reg value} {
    start
    if {![tx [expr {$addr << 1}]] || ![tx $reg] || ![tx $value]} {
        error "Register write NACK"
    }
    stop
}
proc signed_word {bytes offset} {
    set n [expr {([lindex $bytes $offset] << 8) | [lindex $bytes [expr {$offset + 1}]]}]
    if {$n >= 32768} { set n [expr {$n - 65536}] }
    return $n
}

init
reset halt
set result [catch {
    # RM0008 section 7.3.7 RCC_APB2ENR: enable GPIOB, preserving other clocks.
    set en [lindex [read_memory 0x40021018 32 1] 0]
    mww 0x40021018 [expr {$en | 8}]
    # RM0008 section 9.2.5 GPIOB_BSRR: release PB6/PB7 before configuring OD.
    mww 0x40010c10 0xC0
    # RM0008 section 9.2.1 GPIOB_CRL: PB6/PB7 GP open-drain, 2 MHz (nibble 6).
    set crl [lindex [read_memory 0x40010c00 32 1] 0]
    mww 0x40010c00 [expr {($crl & 0x00FFFFFF) | 0x66000000}]
    # UM10204 section 3.1.16: nine recovery clocks and a STOP.
    for {set i 0} {$i < 9} {incr i} { line 6 0; scl_high }
    stop
    echo "PROBE idle SCL=[level 6] SDA=[level 7]"
    foreach addr {0x3c 0x68 0x69} {
        echo [format "PROBE address=0x%02X ACK=%d" $addr [probe $addr]]
    }
    foreach addr {0x68 0x69} {
        if {![probe $addr]} { continue }
        set who [lindex [read_regs $addr 0x75 1] 0]
        echo [format "PROBE address=0x%02X WHO_AM_I=0x%02X" $addr $who]
        if {$who != 0x70 && $who != 0x71 && $who != 0x73} {
            echo "PROBE unsupported identity; no configuration writes"
            continue
        }
        # MPU-9250/6500 register maps: save settings before temporary changes.
        set saved {}
        foreach reg {0x6b 0x6c 0x1c} {
            lappend saved [list $reg [lindex [read_regs $addr $reg 1] 0]]
        }
        set sample_result [catch {
            # PWR_MGMT_1: awake, clock source 1; PWR_MGMT_2: all axes enabled.
            # ACCEL_CONFIG: +/-2 g, 16384 counts/g. Preserve unrelated settings.
            write_reg $addr 0x6b 1
            write_reg $addr 0x6c 0
            set accel_old [lindex [lindex $saved 2] 1]
            write_reg $addr 0x1c [expr {$accel_old & 0xE7}]
            sleep 100
            echo "PROBE config PWR1/PWR2=[read_regs $addr 0x6b 2] ACCEL_CONFIG=[read_regs $addr 0x1c 1]"
            for {set sample 0} {$sample < 8} {incr sample} {
                # ACCEL_XOUT_H .. ACCEL_ZOUT_L: one coherent 6-byte burst.
                set bytes [read_regs $addr 0x3b 6]
                echo [format "PROBE sample=%d ax=%d ay=%d az=%d" $sample \
                    [signed_word $bytes 0] [signed_word $bytes 2] [signed_word $bytes 4]]
                sleep 100
            }
        } sample_error]
        # Restore power state last so any prior sleep state does not block setup.
        foreach entry [list [lindex $saved 2] [lindex $saved 1] [lindex $saved 0]] {
            write_reg $addr [lindex $entry 0] [lindex $entry 1]
        }
        if {$sample_result} { error $sample_error }
    }
} message]
if {$result} { echo "PROBE ERROR: $message" }
# Release bus if possible, then restore normal firmware by reset (no flash writes).
catch {stop}
reset run
shutdown
