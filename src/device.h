
#ifndef DEVICE_H
#define DEVICE_H

enum device {
	DEVICE_UNKNOWN = 0,
	DEVICE_ANY,   /* Unspecified / Any device */
	DEVICE_SU_18, /* Nokia 770 */
	DEVICE_RX_34, /* Nokia N800 */
	DEVICE_RX_44, /* Nokia N810 */
	DEVICE_RX_48, /* Nokia N810 WiMax */
	DEVICE_RX_51, /* Nokia N900 */
};

enum device device_from_string(const char * device);
const char * device_to_string(enum device device);

#endif
