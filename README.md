# BlueFox

![blueFoxLogo_1](https://github.com/user-attachments/assets/2a109334-0cc3-4443-9922-7cc745750f51)

**DISCRIPTION:** BlueFox is a modular mesh network system that uses ESP-32 CAM's that take images of a location every 30 minutes then transmit them back to my home computer via LoRa protocol. The network comprises of end nodes (the ESP CAMs); relay nodes (the LoRa-32 cases outside of my house); and the central node (the LoRa-32 at my house). Due to the limited bandwidth of LoRa and the relitave size of the images I decided to change the image to FRAMESIZE_QQVGA in order to make the data needing to be sent to be smaller the code can send larger images it just takes a longer time due to the greater number of packets needing to be sent. Even with the smaller images LoRa cannot send the image with one radio.transmit() so I created hex packets with a header for identification, ending for termination, and checksums to insure all data was received correctly. With this each relay node will collect all the packets for the last one and then move it forward untill it reaches the end. One at the central node it will hex dump all of the data removing headers, endings, and checksums to be copied in to a python script that will remake the image from the hex. Each node is held in a 3D printed PETG case which is weatherproof and has a 5 dBi antenna for increased range.


**Process:** orgininaly the plan was to have the CAM unit be connected to a STM32 microboard which then connected to a LoRa-32 for sending. However this was scrapped due to the large size and wiring which could become seperated in extreme enough weather conditions. It was then decided to use the ESP-32's onboard WiFi chip which allows one board to be the host for another. This choice proved to be optimal due to the lack of physical constraints and WiFi's high bandwidth which allowed for a smaller case with better resistance to disturbances to the case.  


**STM + CAM + LoRa idea:**

![IMG_5026](https://github.com/user-attachments/assets/092bd56f-ccf5-480e-b03f-a6c85ad27a57)


**Just CAM + LoRa idea:**

![IMG_4964](https://github.com/user-attachments/assets/5ae74ef4-5cc2-4cb0-80b3-716e9a74480a)

**Testing:** in order to make sure the boxes could hold up to the weather and not short circut in extreme weather conditions I created test boxes. These boxes held a voltage stepper, which was only used becasue it was a small, expendible,and had a LED which could be used to determine if the board survived and could still function. The box also contained a small bit of tissue to indicate whether or not water got into the box or not. I created two boxes PLA (the control group named reaper) and PETG (higher streght material named terminator). These boxes where then ratchet strapped to stands and left outside for a week. The weather during this week included 3 thunderstorms, a flood watch, and two 85 degree sunney days giving the boxes a good test set. Additionaly the boxes where put in a sauna for 30 minutes at 170 degrees.

![IMG_5015](https://github.com/user-attachments/assets/2b66cded-ef96-4e57-9c45-e6cf362875e0)
![IMG_5013](https://github.com/user-attachments/assets/613a8f38-cc9e-4903-a605-0f2345dd401b)
![IMG_5010](https://github.com/user-attachments/assets/d8c0eec4-91af-41ab-bb16-57c10546de40)
![IMG_5012](https://github.com/user-attachments/assets/c331d1b0-5a7a-4465-a9ec-a2e5bab8deec)
![IMG_5019](https://github.com/user-attachments/assets/10b40ba9-ac11-4363-bf20-5f8174ded11c)
![IMG_5021](https://github.com/user-attachments/assets/fc34c490-c5b8-426f-9c02-de3aa74ac02c)

**RESULTS:** after the week was done the boxes where removed and opened to test the hardware. The tissue was damp on the PLA box and had a few drop marks on the PETG box. Both boxes voltage steppers LED's worked indicating the boxes did there intended job. In the sauna the PLA box did not melt but became extremely soft to the point where you could push a hole in it with almost no effort while the PETG box held firm as expected due to it's higher melting point.

**FINAL MAKE:** with the test complete and the code working I created the final designs to hold the boards, batteries, antennas, and camera. The range of each mesh is around 0.6 miles in a suburban highley wooded area but is dramatically increased if the the senders hight is increased.


<img width="1304" alt="Screenshot 2025-05-31 at 7 55 28 PM" src="https://github.com/user-attachments/assets/18e7a48c-8c6b-4e4a-9311-3697add5099a" />


**EXTRA NOTES:** When printing with the screws I highly recommend giving youself tolerances as it is very easy to make a screw too big for the hole, I usually use a .2 - .3 mm offset. Additionally, when programming the LoRa it is very important to make sure you are using the Library that matches the chip on your board originaly when I was trying to use the LoRa.h library I was missing packets and getting false data I had to change to the LoRaWan_APP.h library to get my messages to work coherently.

**INSPERATION:** the cases are each named after a top gun character as maverick is the main character it is the CAM, goose is the wing man and relaying the data, and iceman is the commander and most reliable.
The name blueFox comes from a cold war project that built covert long range image and data transmittion. Hense why this project is named blueFox because it's intention is to be a covert camera sytem that can send images over long ranges.


![ChatGPT_Image_Jun_1_2025_at_01_08_06_PM](https://github.com/user-attachments/assets/46a17720-6489-43d5-86ae-fc04934339f1)
