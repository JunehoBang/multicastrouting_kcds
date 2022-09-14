# multicastrouting_kcds

This repository contains the QualNet code for simulating the kcds based multicast routing algorithm 
in the military tactical network where a number of personal devices are moving as a group 
while the inter-group communication is intermediated by the static network (wireless mesh network)

While the multicast_kcds.m implements the virtual backbone based multicast data delivery, the borderingprotocol.h 
implements the data packet transaction between the mobile node and the static mesh router

It was implemented for demo for the researchers in the funding association, 
and details of the technical matters are specified in Ref.[^1]

[^1]: J. Bang et. al., **"Constructing κ-redundant Data Delivery Structure for Multicast in a Military Hybrid Network,"** *Journal of the Korea Institute of Military Science and Technology*, vol. 15, no. 6, pp. 770-778, Dec. 2012
