# aic-nixie

> An ESP8266-based 6-digit IN-12B nixie counter that queries the Art Institute of Chicago's API

<a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/00-hero.jpg">
    <img src="images/photos/00-hero-tn.jpg">
</a>

While working as a web developer at the Art Institue of Chicago, I got the idea that it would be neat to have some physical manifestation of the work we were doing. My work at the time focused on [interconnecting systems](https://mw19.mwconf.org/paper/building-a-data-hub-microservices-apis-and-system-integration-at-the-art-institute-of-chicago/) and creating a [public API](https://www.artic.edu/open-access/public-api), so I decided to make a counter that would show the number of artworks that have been updated in our API each day. Further, I wanted the counter to be a piece of art in its own right. It had to feel high quality, analog, and hand made.

Long story short, what you find here is the end result. My nixie counter connects to Wi-Fi using an ESP8266. It queries a Python microservice that's running on e.g. my desktop computer and displays whatever number gets returned. This allows me to easily change what the counter displays, without having to reflash it.

Over time, what this project represents has shifted in practice to be less about the Art Institute's data and more about my own workflows. For example, yes, it can display [artworks updated since 9 AM CT](server/aic.py), but it can also display the [current word count of a Markdown file](server/wc.py). But the core idea of having a physical manifestation of some digital count with an emphasis on measuring work has stayed constant.

This repository contains a write-up of the project, along with designs for the enclosure and all related code. This was my first woodworking and Arduino project. As such, this write-up is written from the perspective of a beginner. Some parts here may seem obvious to more seasoned makers. If you spot any mistakes or misconceptions, please [open an issue](https://github.com/IllyaMoskvin/aic-nixie/issues).



## Table of Contents

 * [Photo Gallery](#photo-gallery)
 * [Inspiration](#inspiration)
 * [Design](#design)
 * [Components](#components)
   * [Nixie Tubes: IN-12B](#nixie-tubes-in-12b)
   * [Doayee's Nixie Driver](#doayees-nixie-driver)
   * [Acrylic-Mounted PCBs](#acrylic-mounted-pcbs)
     * [Adafruit HUZZAH ESP8266 Breakout](#adafruit-huzzah-esp8266-breakout)
     * [HV PSU: YanZeyuan’s NCH6100HV](#hv-psu-yanzeyuans-nch6100hv)
     * [LV PSU: DROK LM2596](#lv-psu-drok-lm2596)
 * [Assembly](#assembly)
   * [Acrylic Plate for PCBs](#acrylic-plate-for-pcbs)



## Photo Gallery

<table>
    <tr>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/01-front.jpg">
                <img src="images/photos/01-front-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/02-three-quarters.jpg">
                <img src="images/photos/02-three-quarters-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/03-side.jpg">
                <img src="images/photos/03-side-tn.jpg">
            </a>
        </td>
    </tr>
    <tr>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/04-back.jpg">
                <img src="images/photos/04-back-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/05-bottom.jpg">
                <img src="images/photos/05-bottom-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/06-button.jpg">
                <img src="images/photos/06-button-tn.jpg">
            </a>
        </td>
    </tr>
    <tr>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/07-lid-off.jpg">
                <img src="images/photos/07-lid-off-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/08-inside-left.jpg">
                <img src="images/photos/08-inside-left-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/09-inside-right.jpg">
                <img src="images/photos/09-inside-right-tn.jpg">
            </a>
        </td>
    </tr>
</table>



## Inspiration

Without exaggeration, I looked at hundreds of nixie clocks while designing this counter. The most direct inspiration for this project turned out to be a clock made by [NixieDream](https://www.etsy.com/shop/NixieDream) on Etsy. I don't know when exactly this clock was made, but by the time I found it through Pinterest in mid-February 2019, it had already been sold.

<img src="images/NixieDream.jpg">

What did I like about this design? I loved the idea of having the front face of the counter tilt backwards. This way, when the counter was placed on a tabletop, it would face a sitting or standing viewer more head-on than if the front face was simply perpendicular to the table's surface. I liked how the grain of the wood on the sides of this clock emphasized its directionality. Overall, I liked its angles and proportions.

That said, there were things about it I wanted to change. For one, I did not like how deep the nixie tubes sat within the front face. Each nixie tube has a stack of digits in it. Watching the digits change conveys a sense of dimensionality, which gets lost somewhat when the tubes are set so deep. I wanted to clean up the design by moving the button(s) to the rear and hiding the seams between the pieces of wood. Lastly, I wanted to reduce the footprint of this design by foreshortening the back, giving it a more square profile.

Taken in sum, these changes promised to be a challenge, both in terms of woodworking and component layout. Through these changes, my intention was to make my counter stand on its own—in conversation with NixieDream's clock, but not merely a remix. Here's hoping that I succeeded!



## Design

<img src="images/design.png">

This counter was designed entirely in Adobe Illustrator CC. At the time, I was not proficient with 3D CAD software. I have no interest in making this design particularly easy to reproduce, but I'm willing to share the files:

 * [side.ai](designs/side.ai)
 * [back.ai](designs/back.ai)
 * [front.ai](designs/front.ai)

I made an effort to organize their contents, but they are provided as-is. There are definitely some inaccuracies relative to the finished counter. For example, you may notice that the acrylic piece was designed to mount a fourth PCB—this [TXS0108E 8-bit bi-directional level-shifter breakout](https://www.addicore.com/TXS0108E-p/ad284.htm). Turns out, it wasn't needed? Also, I used a different button than the one measured for the designs. Generally, the measurements for the back cutouts don't quite reflect the final product.

There are a couple things I'd change here, too. I'd make the two Forstner holes slightly bigger, especially for the button. I'd maybe make the indentations for the #6 nuts slightly bigger, too. While cleaning up the design files, I noticed that all of the holes on the back should have been at least 0.5mm higher. I haven't noticed it while tinkering with the counter for the past year or so, but yeah, that flaw made it into the end product. Unfortunate, but in a way, that makes it more human.

In terms of the design process, it was just a cycle of measuring things with digital calipers, nudging vectors around, and doing mental math. I made a paper prototype at one point to get a sense of scale and to sanity-check that the nixie tubes would indeed fit into the cutouts. Overall, I think the final product turned out to be very close to the design, [which is great](https://www.oxfordreference.com/view/10.1093/acref/9780199539536.001.0001/acref-9780199539536-e-1418).

Please note that the pattern for the front panel cutout was derived from [IN-12.svg](http://reboots.g-cipher.net/time/IN-12.svg) shared by [Engram Enterprises](http://reboots.g-cipher.net/) as part of their [Amy Time! Nixie Tube Clock](http://reboots.g-cipher.net/time/) project. Thank you!



## Components

As a newcomer to electronics, I knew that I'd have to use off-the-shelf parts where possible, within reason. I had to balance learning new things versus getting this project done, and there was plenty to learn here already. Plus, nixie tubes are high-voltage devices. I wanted to minimize the risk of harming components or myself.


### Nixie Tubes: IN-12B

This counter uses [IN-12B](http://www.swissnixie.com/tubes/IN12B/) tubes, made in the USSR in 1985. "Nixie" is a generalized trademark, originally filed by the [Burroughs Corporation](http://www.jb-electronics.de/html/elektronik/nixies/n_nixie_geschichte.htm?lang=en) in 1956. It's a type of [cold-cathode tube](https://www.explainthatstuff.com/how-nixie-tubes-work.html), related to neon tubes. There's a stack of metal digits in each tube. Each digit is connected to a small pin that sticks out of the tube, plus one pin for power. Once positive voltage is applied to the power pin (anode), we can make any digit light up by connecting its pin to ground (cathode).

Nixies haven't really been mass-produced since the 80s, but there's still a fair bit of "new old stock" (NOS) kicking around, and there's a few people trying to revive the art of manufacturing these. There's a definite niche for nixies in industrial art projects like this one. As a result, there's currently a lot of knowledge online about nixies.

For this project, I wanted to use [end-view tubes](http://www.tube-tester.com/sites/nixie/trade03-nixie-tubes.htm) that were cheap and had [good community support](https://hackaday.io/search?term=nixie). Originally, I looked at the [IN-1](http://www.tube-tester.com/sites/nixie/data/in-1/in-1.htm) or the [IN-12A](http://www.tube-tester.com/sites/nixie/data/in-12a.htm). Once I found Doayee's driver, [IN-12B](http://www.tube-tester.com/sites/nixie/data/in-12b.htm) was an easy choice. Having a decimal point was an unexpected bonus, which allowed me to show IP addresses via the nixies.


### Doayee's Nixie Driver

<img src="images/doayee-nixie-driver.jpg">

[Doayee](https://doayee.co.uk/) is a small bespoke electronic project store and hobbyist blog based in the UK. They created a [Nixie Tube Driver](https://doayee.co.uk/nixie/), which they [Kickstarted](https://www.kickstarter.com/projects/doayee/nixie-tube-driver) in 2017 and have subsequently offered for sale via [Tindie](https://www.tindie.com/products/Doayee/nixie-tube-driver/). This driver is what made my project possible. Nothing else had the perfect combination of form factor, ready availability, and Arduino library support.

That said, I ran into some issues, too. But before I get into all that, I just want to express how awesome the Doayee guys were at providing support. I reached out to them via email, and they did their genuine best to help me debug things. We were unsuccessful, but at some point, that doesn't matter. The effort they put into helping a newbie went above and beyond what I'd expect from a shop of thier size.

Unfortunately, I went through _three_ of these boards while working on this project. For the most part, I have only myself to blame. My first board, I shorted HV to 5V while measuring voltage. The second board, some strange issues appeared after a few weeks of operation. The third board is still going strong. Lesson learned: if you are new to electronics, order extra parts!

The second board had to be retired because the "4" digit in one of the tubes [refused to turn off](https://www.dropbox.com/s/mgjyzf93gjzlxyp/IMG_0748.MOV?dl=0). This issue did not surface until about 50 hours of operation. I asked Doayee about it, and though they tried to help me debug the problem, we were unsuccessful. It seems that something caused the "4" pin to become grounded. Possible solder mask damage?

I'm not really knowledgable enough yet to speculate on the matter, but tenatively, I think that some of the traces come too close to the through-holes for the [PL31A-P](https://www.steampunkalchemy.com/en/nixie-tubes/sockets-pins-driver-ic) socket pins. My best guess so far is that I might have damaged the solder mask of an adjacent trace while soldering a pin, creating a short. I don't know why it would take 50 hours of operation for that to manifest itself, though. Maybe the heat from the nixies served to further degrade the solder mask? They don't run hot, but they do run warmer than room temperature.

According to the documentation, this driver operates at a 5V [logic level](https://learn.sparkfun.com/tutorials/logic-levels/all). Most Arduino devices nowadays use 3.3V logic. That includes the ESP8266. I thought I'd need to convert between the two logic levels. I tried using a [TXS0108E](https://www.addicore.com/TXS0108E-p/ad284.htm) level shifter between the ESP8266 and the driver. This bugged out on me in ways that were difficult to diagnose. Sometimes, digits would flicker or refuse to light. Other times, all of the segments would light up at the same time—not all at once, but incrementally, until every segment on every tube was lit. My best guess is that the level shifter interfered with the way the clock signal was being trigged manually by the driver library to send data.

Eventually, I realized that the [74LVC1G79](https://assets.nexperia.com/documents/data-sheet/74LVC1G79.pdf) and [HV5122](http://ww1.microchip.com/downloads/en/DeviceDoc/20005418B.pdf) on the driver board work just fine with 3.3V logic, so I tried it without the level shifter. The nixies seemed to work fine without it. I found that [/u/wkrp28 on Reddit](https://old.reddit.com/r/arduino/comments/83s09b/ive_always_wanted_a_nixie_clock_and_finally_made/) had already build a clock using Doayee's driver and the ESP8266 without level shifting. I asked Doayee if the board could handle 3.3V logic. They said that operating with 5V Vdd and 3.3V signal lines comes quite close to the Vih threshold of Vdd-2.0, which might cause errors, but it is highly unlikely to damage components.

Lastly, it seems that Pin 12 on the left-most tube is not connected to anything, so if you use an [IN-12B](http://www.tube-tester.com/sites/nixie/data/in-12b.htm) there, you can't light up the decimal point in that tube. Having the ability to light up that decimal point might have made the IP scroll animation a smidge smoother. Looking at the pinouts for [IN-15A](http://www.tube-tester.com/sites/nixie/data/in-15a.htm) and [IN-15B](http://www.tube-tester.com/sites/nixie/data/in-12a.htm), this shouldn't interfere with displaying any symbols.

Also, it's worthwhile to note that Doayee's driver includes LED backlights for each tube. I decided that RGB backlighting would not make a good match for the all-wood case design, so I stopped my attempts to make them work pretty early on in the project, but that decision was also made out of caution. While the nixie-related components seem to work fine on 3.3V logic, these LEDs are color-controlled via a PWM signal from the pins. From what I can tell, connecting LEDs meant for 5V to 3.3V PWM signals causes them to be much dimmer than they were meant to be.

Speaking of LEDs, there's a single, stand-alone blue LED on the driver board that I think is simply meant to be an indicator that the board is recieving power. That LED is ridiculously bright. Out-of-the-box, you could see its light leaking out behind the tubes, even in a bright room. To fix this, I covered the LED with several layers of opaque black nail polish.



### Acrylic-Mounted PCBs

<img src="images/acrylic-pcbs.jpg">

Aside from the nixie driver, all of the other PCBs are mounted on a piece of laser-cut acrylic that's easy to remove from the enclosure for reprogramming. I'll discuss the particulars of this design in more depth [below](#acrylic-plate-for-pcbs), but for now, here is a photo of the PCBs for reference. From left to right:

  * [Adafruit HUZZAH ESP8266 Breakout](#adafruit-huzzah-esp8266-breakout)
  * [YanZeyuan’s NCH6100HV](#hv-psu-yanzeyuans-nch6100hv)
  * [DROK LM2596](#lv-psu-drok-lm2596)


#### Adafruit HUZZAH ESP8266 Breakout

The [Adafruit HUZZAH ESP8266 Breakout](https://www.adafruit.com/product/2471) is the Arduino "brain" of the counter. There are many [ESP8266 development boards](https://makeradvisor.com/best-esp8266-wi-fi-development-board/) out there. I chose this one for the form factor, pinout, and standoffs. It seemed like the just-enough-frills option. I liked that it could handle being connected to two sources of power with no trouble. That made it possible to flash the counter and monitor serial output while leaving it plugged into the wall.

At the time, I wanted a board without a USB output, since I thought that I wouldn't need it. In retrospect, it probably wouldn't have made any difference if I had a board with a USB socket instead of FTDI pins. In any case, I had no trouble using an [Adafruit FTDI Friend](https://www.adafruit.com/product/284) to program the device.

For a Wi-Fi-enabled Arduino-compatible solution, boards built on the ESP8266 or ESP32 chips seem to be the best options, with regard to price and community support. I haven't felt the need to experiment with an ESP32 yet. The ESP8266 was enough for this project.


#### HV PSU: YanZeyuan’s NCH6100HV

Nixie tubes are high-voltage, low-current devices. For the IN-12B, each lit digit only draws about 2.5-3.0 mA, but it takes about 170V strike voltage to light up that digit, and about 140V to sustain it. So we need a specialized high voltage power supply to convert the wall-wart voltage up to what our nixies require.

For this project, I used [YanZeyuan’s NCH6100HV](https://www.nixieclock.org/?p=493). I learned about it via [Kevin Lee's "Nixie Tube Clock"](https://0x7d.com/2017/nixie-tube-clock/#HV_Power_Supply) project log. His write-up contains some great information about building a power supply from scratch. As a newbie, most of it is way over my head.

The NCH6100HV was perfect for this project. It had a small footprint and two M2 mounting holes for standoffs, so it was easy to attach to the acrylic. It came with solderless terminal blocks, which eased assembly. Its output voltage could be adjusted to mitigate overheating. The recommended input power supply for it was DC12V 2A. After doing the math to account for the other components, this was actually enough to power the entire counter.


#### LV PSU: DROK LM2596

The NCH6100HV takes care of powering the nixies, but we still need something to power the nixie driver and the ESP8266. The [DROK LM2596](https://www.droking.com/LM2596-Immersion-Gold-Power-Supply-Module-DC-3V-40V-to-1.23-37V-3A-Regulator-DC-12V-24V-Adapter-Switching-Power-Supply) is an adjustable DC-to-DC switching power supply. It can accept DC 3V though 40V as input and output DC 1.23V through 37V. I'm inputting 12V and outputting 5V, which then gets split via wire splice between the Doayee driver and the ESP8266.

To be honest, I chose this specific unit because it looked nice. From what I knew, it ticked all the boxes in terms of electrical specs, and it had the right form factor and stand-off holes for mounting on the acrylic—but so did a lot of other units like it. This one looked nice to boot. It also ran cool, which is a problem for [some voltage converters](https://electronics.stackexchange.com/questions/251914/lm2596-buck-converter-overheats-converting-36dc-5dc-at-600ma).

Similar to the Doayee nixie driver, this board had an LED to indicate that it was receiving power. It was also too bright, causing some light to leak through the tubes. I covered it up with some black nail polish.



## Assembly

I neglected to take in-progress photos while making the counter, but these should help illustrate how it was assembled:

<table>
    <tr>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/07-lid-off.jpg">
                <img src="images/photos/07-lid-off-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/10-nixie-mounts.jpg">
                <img src="images/photos/10-nixie-mounts-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/11-acrylic-plate.jpg">
                <img src="images/photos/11-acrylic-plate-tn.jpg">
            </a>
        </td>
        <td>
            <a href="https://raw.githubusercontent.com/IllyaMoskvin/aic-nixie/master/images/photos/12-screw-indents.jpg">
                <img src="images/photos/12-screw-indents-tn.jpg">
            </a>
        </td>
    </tr>
</table>

The actual steps for the assembly were pretty typical from a laser cutting and woodworking perspective. In this section, I'll try to skim over the basics and focus on the interesting stuff: the "gotchas" and the design considerations.


### Acrylic Plate for PCBs

As mentioned in [Components](#acrylic-mounted-pcbs), all of the PCBs aside from the driver were mounted on a laser-cut piece of 3mm acrylic using 6mm nylon standoffs, nuts, and screws: 2 x M2 for NCH6100HV, 4 x M2.5 for ESP8266, and 4 x M3 for LM2596. I used the laser to etch indentations for two #6-32 nuts into the acrylic. These nuts were used to attach the acrylic plate to the inside of the enclosure via two decorative screws. This assembly was actually the first part I fabricated, before I started working on the wood enclosure.

The design intent behind mounting the PCBs to a separate component was threefold. First, I didn't trust myself to drill the pilot holes for the standoffs directly into the wood with enough precision. Secondly, I wanted to make it easy to pull the ESP8266 out of the enclosure, while leaving it connected, allowing easy access to FTDI pins and SMD buttons for development. Thirdly, I liked that this approach allowed the acrylic piece to be easily modified, without affecting the rest of the enclosure.

Unfortunately, this last point didn't really work out how I expected: during design, I forgot to account for the fact that the screw heads would stick out, pushing the acrylic piece away from the wood to which it was meant to be mounted. Before the glue-up, this was easy to remedy by drilling some indentations into the wood, but now that the counter is assembled, it would be difficult to use the same fix, if I wanted to change the acrylic piece.

One solution might be to engrave circular indentations for the screw heads on the other side of the acrylic, similar to what was done for the #6 nuts. Registration could be an issue, but in this case, we care more about depth than XY alignment. The indentations could be made oversized to leave room for error. The acrylic might need to be thicker, too.


*To be continued...*
