/***************************************************************************//**
 *   @file   iio_axi_dac_app.c
 *   @brief  Implementation of iio_axi_dac_app.
 *   This application instantiates iio_axi_adc and iio_axi_dac devices, for
 *   reading/writing and parameterization.
 *   @author Cristian Pop (cristian.pop@analog.com)
********************************************************************************
 * Copyright 2019(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/

#include "error.h"
#include "iio.h"
#include "iio_axi_dac_app.h"
#include "xml.h"

/******************************************************************************/
/*************************** Types Declarations *******************************/
/******************************************************************************/

/**
 * @struct iio_axi_dac
 * @brief Structure with references to DAC and DMA cores.
 */
struct iio_axi_dac {
	/** Pointer to "axi_dac" instance */
	struct axi_dac *dac;
	/** Pointer to "axi_dmac" instance. */
	struct axi_dmac *dmac;
	/** Address used by DMA, for sending data to device */
	uint32_t dac_ddr_base;
	/** Function pointer to flush the data cache for the given address range */
	void (*dcache_flush_range)(uint32_t address, uint32_t bytes_count);
};

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/

/**
 * @brief get_dds_calibscale().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Length of chars written in buf, or negative value on failure.
 */
static ssize_t get_voltage_calibscale(void *device, char *buf, size_t len,
				      const struct iio_ch_info *channel)
{
	int32_t val, val2;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_get_calib_scale(iio_dac->dac, channel->ch_num,
			&val, &val2);
	int32_t i = 0;
	if(ret < 0)
		return ret;
	if(val2 < 0 && val >= 0) {
		ret = (ssize_t) snprintf(buf, len, "-");
		if(ret < 0)
			return ret;
		i++;
	}
	ret = i + (ssize_t) snprintf(&buf[i], len, "%"PRIi32".%.6"PRIi32"", val,
				     labs(val2));

	return ret;
}

/**
 * @brief get_dds_calibphase().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_voltage_calibphase(void *device, char *buf, size_t len,
				      const struct iio_ch_info *channel)
{
	int32_t val, val2;
	int32_t i = 0;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_get_calib_phase(iio_dac->dac, channel->ch_num,
			&val, &val2);
	if(ret < 0)
		return ret;
	if(val2 < 0 && val >= 0) {
		i++;
	}
	return i + snprintf(&buf[i], len, "%"PRIi32".%.6"PRIi32"", val, labs(val2));
}

/**
 * @brief get_dds_sampling_frequency().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_voltage_sampling_frequency(void *device, char *buf,
		size_t len,
		const struct iio_ch_info *channel)
{
	/* This function doesn't have an equivalent function in axi_dac_core,
	 * and it should be implemented there first */

	return -ENOENT;
}

/**
 * get_dds_altvoltage_phase().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_altvoltage_phase(void *device, char *buf, size_t len,
				    const struct iio_ch_info *channel)
{
	uint32_t phase;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_get_phase(iio_dac->dac, channel->ch_num, &phase);
	if (ret < 0)
		return ret;

	return snprintf(buf, len, "%"PRIu32"", phase);
}

/**
 * @brief get_dds_altvoltage_scale().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_altvoltage_scale(void *device, char *buf, size_t len,
				    const struct iio_ch_info *channel)
{
	int32_t scale;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_get_scale(iio_dac->dac, channel->ch_num, &scale);
	if (ret < 0)
		return ret;

	return snprintf(buf, len, "%"PRIi32".%.6"PRIi32"", (scale / 1000000),
			(scale % 1000000));
}

/**
 * @brief get_dds_altvoltage_frequency().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_altvoltage_frequency(void *device, char *buf, size_t len,
					const struct iio_ch_info *channel)
{
	uint32_t freq;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_get_frequency(iio_dac->dac, channel->ch_num, &freq);
	if (ret < 0)
		return ret;

	return snprintf(buf, len, "%"PRIi32"", freq);
}

/**
 * @brief get_dds_altvoltage_raw().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_altvoltage_raw(void *device, char *buf, size_t len,
				  const struct iio_ch_info *channel)
{
	/* This function doesn't have an equivalent function in axi_dac_core,
	 * and it should be implemented there first */

	return -ENOENT;
}

/**
 * @brief get_dds_altvoltage_sampling_frequency().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Where value is stored.
 * @param len - Maximum length of value to be stored in buf.
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t get_altvoltage_sampling_frequency(void *device, char *buf,
		size_t len,
		const struct iio_ch_info *channel)
{
	/* This function doesn't have an equivalent function in axi_dac_core,
	 * and it should be implemented there first */

	return -ENOENT;
}

/**
 * @brief set_dds_calibscale().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return Number of bytes written in buf, or negative value on failure.
 */
static ssize_t set_voltage_calibscale(void *device, char *buf, size_t len,
				      const struct iio_ch_info *channel)
{
	float calib= strtof(buf, NULL);
	int32_t val = (int32_t)calib;
	int32_t val2 = (int32_t)(calib* 1000000) % 1000000;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_set_calib_scale(iio_dac->dac, channel->ch_num, val,
			val2);
	if (ret < 0)
		return ret;

	return len;
}

/**
 * @brief set_dds_calibphase().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_voltage_calibphase(void *device, char *buf, size_t len,
				      const struct iio_ch_info *channel)
{
	float calib = strtof(buf, NULL);
	int32_t val = (int32_t)calib;
	int32_t val2 = (int32_t)(calib* 1000000) % 1000000;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_set_calib_phase(iio_dac->dac, channel->ch_num, val,
			val2);
	if (ret < 0)
		return ret;

	return len;
}

/**
 * @brief set_dds_sampling_frequency().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return: Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_voltage_sampling_frequency(void *device, char *buf,
		size_t len,
		const struct iio_ch_info *channel)
{
	/* This function doesn't have an equivalent function in axi_dac_core,
	 * and it should be implemented there first */

	return -ENOENT;
}

/**
 * @brief set_dds_altvoltage_phase().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_altvoltage_phase(void *device, char *buf, size_t len,
				    const struct iio_ch_info *channel)
{
	uint32_t phase = srt_to_uint32(buf);
	struct iio_axi_dac * iio_dac = (struct iio_axi_dac *)device;
	ssize_t ret = axi_dac_dds_set_phase(iio_dac->dac, channel->ch_num, phase);
	if (ret < 0)
		return ret;

	return len;
}

/**
 * @brief set_dds_altvoltage_scale().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_altvoltage_scale(void *device, char *buf, size_t len,
				    const struct iio_ch_info *channel)
{
	float fscale = strtof(buf, NULL);
	int32_t scale = fscale * 1000000;
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_set_scale(iio_dac->dac, channel->ch_num, scale);
	if (ret < 0)
		return ret;

	return len;
}

/**
 * @brief set_dds_altvoltage_frequency().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_altvoltage_frequency(void *device, char *buf, size_t len,
					const struct iio_ch_info *channel)
{
	uint32_t freq = srt_to_uint32(buf);
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret = axi_dac_dds_set_frequency(iio_dac->dac, channel->ch_num, freq);
	if (ret < 0)
		return ret;

	return len;
}

/**
 * @brief set_dds_altvoltage_raw().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return: Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_altvoltage_raw(void *device, char *buf, size_t len,
				  const struct iio_ch_info *channel)
{
	uint32_t dds_mode = srt_to_uint32(buf);
	struct iio_axi_dac* iio_dac = (struct iio_axi_dac*)device;
	ssize_t ret;
	ret = axi_dac_set_datasel(iio_dac->dac, -1,
				  dds_mode ? AXI_DAC_DATA_SEL_DDS : AXI_DAC_DATA_SEL_DMA);
	if (ret < 0)
		return ret;

	return len;
}

/**
 * @brief set_dds_altvoltage_sampling_frequency().
 * @param device - Physical instance of a iio_axi_dac device.
 * @param buf - Value to be written to attribute.
 * @param len - Length of the data in "buf".
 * @param channel - Channel properties.
 * @return Number of bytes written to device, or negative value on failure.
 */
static ssize_t set_altvoltage_sampling_frequency(void *device, char *buf,
		size_t len,
		const struct iio_ch_info *channel)
{
	/* This function doesn't have an equivalent function in axi_dac_core,
	 * and it should be implemented there first */

	return -ENOENT;
}

/**
 * @struct iio_attr_voltage_calibphase
 * @brief Structure for "calibphase" attribute.
 */
static struct iio_attribute iio_attr_voltage_calibphase = {
	/** Attribute name */
	.name = "calibphase",
	/** Read attribute from device */
	.show = get_voltage_calibphase,
	/** Write attribute to device */
	.store = set_voltage_calibphase,
};

/**
 * @struct iio_attr_voltage_calibscale
 * @brief Structure for "calibscale" attribute.
 */
static struct iio_attribute iio_attr_voltage_calibscale = {
	/** Attribute name */
	.name = "calibscale",
	/** Read attribute from device */
	.show = get_voltage_calibscale,
	/** Write attribute to device */
	.store = set_voltage_calibscale,

};

/**
 * @struct iio_attr_voltage_sampling_frequency
 * @brief Structure for "sampling_frequency" attribute.
 */
static struct iio_attribute iio_attr_voltage_sampling_frequency = {
	/** Attribute name */
	.name = "sampling_frequency",
	/** Read attribute from device */
	.show = get_voltage_sampling_frequency,
	/** Write attribute to device */
	.store = set_voltage_sampling_frequency,
};

/**
 * @struct iio_attr_altvoltage_raw
 * @brief Structure for "raw" attribute.
 */
static struct iio_attribute iio_attr_altvoltage_raw = {
	/** Attribute name */
	.name = "raw",
	/** Read attribute from device */
	.show = get_altvoltage_raw,
	/** Write attribute to device */
	.store = set_altvoltage_raw,
};

/**
 * @struct iio_attr_altvoltage_phase
 * @brief Structure for "phase" attribute.
 */
static struct iio_attribute iio_attr_altvoltage_phase = {
	/** Attribute name */
	.name = "phase",
	/** Read attribute from device */
	.show = get_altvoltage_phase,
	/** Write attribute to device */
	.store = set_altvoltage_phase,
};

/**
 * @struct iio_attr_altvoltage_frequency
 * @brief Structure for "frequency" attribute.
 */
static struct iio_attribute iio_attr_altvoltage_frequency = {
	/** Attribute name */
	.name = "frequency",
	/** Read attribute from device */
	.show = get_altvoltage_frequency,
	/** Write attribute to device */
	.store = set_altvoltage_frequency,
};

/**
 * @struct iio_attr_altvoltage_scale
 * @brief Structure for "scale" attribute.
 */
static struct iio_attribute iio_attr_altvoltage_scale = {
	/** Attribute name */
	.name = "scale",
	/** Read attribute from device */
	.show = get_altvoltage_scale,
	/** Write attribute to device */
	.store = set_altvoltage_scale,
};

/**
 * @struct iio_attr_altvoltage_sampling_frequency
 * @brief Structure for "sampling_frequency" attribute.
 */
static struct iio_attribute iio_attr_altvoltage_sampling_frequency = {
	/** Attribute name */
	.name = "sampling_frequency",
	/** Read attribute from device */
	.show = get_altvoltage_sampling_frequency,
	/** Write attribute to device */
	.store = set_altvoltage_sampling_frequency,
};

/**
 * List containing attributes, corresponding to "voltage" channels.
 */
static struct iio_attribute *iio_voltage_attributes[] = {
	&iio_attr_voltage_calibscale,
	&iio_attr_voltage_calibphase,
	&iio_attr_voltage_sampling_frequency,
	NULL,
};

/**
 * List containing attributes, corresponding to "altvoltage" channels.
 */
static struct iio_attribute *iio_altvoltage_attributes[] = {
	&iio_attr_altvoltage_raw,
	&iio_attr_altvoltage_phase,
	&iio_attr_altvoltage_scale,
	&iio_attr_altvoltage_frequency,
	&iio_attr_altvoltage_sampling_frequency,
	NULL,
};

/**
 * @brief Transfer data from RAM to device.
 * @param iio_inst - Physical instance of a iio_axi_dac device.
 * @param bytes_count - Number of bytes to transfer.
 * @param ch_mask - Opened channels mask.
 * @return Number of bytes transfered, or negative value in case of failure.
 */
static ssize_t iio_axi_dac_transfer_mem_to_dev(void *iio_inst,
		size_t bytes_count,
		uint32_t ch_mask)
{
	struct iio_axi_dac *iio_dac = iio_inst;
	ssize_t ret;

	if(iio_dac->dcache_flush_range)
		iio_dac->dcache_flush_range(iio_dac->dac_ddr_base, bytes_count);

	iio_dac->dmac->flags = DMA_CYCLIC;
	ret = axi_dmac_transfer(iio_dac->dmac, iio_dac->dac_ddr_base,
				bytes_count);
	if(ret < 0)
		return ret;

	return bytes_count;
}

/**
 * @brief Write chunk of data into RAM.
 * This function is probably called multiple times by libtinyiiod before a
 * "iio_transfer_mem_to_dev" call, since we can only write "bytes_count" bytes
 * at a time.
 * @param iio_inst - Physical instance of a iio_axi_dac device.
 * @param buf - Values to write.
 * @param offset - Offset in memory after the nth chunk of data.
 * @param bytes_count - Number of bytes to write.
 * @param ch_mask - Opened channels mask.
 * @return bytes_count or negative value in case of error.
 */
static ssize_t iio_axi_dac_write_dev(void *iio_inst, char *buf,
				     size_t offset,  size_t bytes_count, uint32_t ch_mask)
{
	struct iio_axi_dac *iio_dac = iio_inst;
	ssize_t ret, i;

	for (i = 0; i < iio_dac->dac->num_channels; i++) {
		ret = axi_dac_set_datasel(iio_dac->dac, i,
					  (BIT(i) & ch_mask) ? AXI_DAC_DATA_SEL_DMA : AXI_DAC_DATA_SEL_DDS);
		if(ret < 0)
			return ret;
	}

	ret = axi_dac_set_buff(iio_dac->dac, iio_dac->dac_ddr_base + offset,
			       (uint16_t *)buf,
			       bytes_count);
	if(ret < 0)
		return ret;

	return bytes_count;
}

enum ch_type {
	CH_VOLTGE,
	CH_ALTVOLTGE,
};

/**
 * @brief Fill device with channels.
 * @param device - Node to populate with channels.
 * @param ch_no - Number of channels to be added to "device" element.
 * @param ch_t - Channel type.
 * @return SUCCESS in case of success or negative value otherwise.
 */
static ssize_t iio_axi_dac_channel_xml(struct xml_node *device, uint8_t ch_no,
				       enum ch_type ch_t)
{
	char *ch_id[] = {"voltage", "altvoltage"};
	char *ch_name[] = {"_I_F", "_Q_F"};
	struct iio_attribute **iio_attributes;
	struct xml_node *attribute, *channel;
	struct xml_attribute *att = NULL;
	char buff[256];
	uint8_t i, j;
	ssize_t ret;

	for (i = 0; i < ch_no; i++) {
		ret = xml_create_node(&channel, "channel");
		if (ret < 0)
			return ret;
		ret = sprintf(buff, "%s%d", ch_id[ch_t], i);
		if (ret < 0)
			return ret;
		ret = xml_create_attribute(&att, "id", buff);
		if (ret < 0)
			return ret;
		ret = xml_add_attribute(channel, att);
		if (ret < 0)
			return ret;
		ret = xml_create_attribute(&att, "type", "output");
		if (ret < 0)
			return ret;
		ret = xml_add_attribute(channel, att);
		if (ret < 0)
			return ret;

		if (ch_t == CH_VOLTGE) {
			ret = xml_create_node(&attribute, "scan-element");
			if (ret < 0)
				return ret;
			ret = sprintf(buff, "%d", i);
			if (ret < 0)
				return ret;
			ret = xml_create_attribute(&att, "index", buff);
			if (ret < 0)
				return ret;
			ret = xml_add_attribute(attribute, att);
			if (ret < 0)
				return ret;
			ret = xml_create_attribute(&att, "format", "le:S16/16&gt;&gt;0");
			if (ret < 0)
				return ret;
			ret = xml_add_attribute(attribute, att);
			if (ret < 0)
				return ret;
			ret = xml_add_node(channel, attribute);
			if (ret < 0)
				return ret;
		} else {
			/* CH_ALTVOLTGE */
			ret = sprintf(buff, "TX%d%s%d", (i / 4) + 1, ch_name[(i % 4) / 2], (i % 2) + 1);
			if (ret < 0)
				return ret;
			ret = xml_create_attribute(&att, "name", buff);
			if (ret < 0)
				return ret;
			ret = xml_add_attribute(channel, att);
			if (ret < 0)
				return ret;
		}
		iio_attributes = (ch_t == CH_VOLTGE) ? iio_voltage_attributes :
				 iio_altvoltage_attributes;

		for (j = 0; iio_attributes[j] != NULL; j++) {
			ret = xml_create_node(&attribute, "attribute");
			if (ret < 0)
				return ret;
			ret = xml_create_attribute(&att, "name",
						   iio_attributes[j]->name);
			if (ret < 0)
				return ret;
			ret = xml_add_attribute(attribute, att);
			if (ret < 0)
				return ret;
			ret = sprintf(buff, "out_%s%d_%s", ch_id[ch_t], i, iio_attributes[j]->name);
			if (ret < 0)
				return ret;
			ret = xml_create_attribute(&att, "filename", buff);
			if (ret < 0)
				return ret;
			ret = xml_add_attribute(attribute, att);
			if (ret < 0)
				return ret;
			ret = xml_add_node(channel, attribute);
			if (ret < 0)
				return ret;
		}
		ret = xml_add_node(device, channel);
		if (ret < 0)
			return ret;
	}

	return SUCCESS;
}

/**
 * @brief Get an axi_dac xml.
 * @param xml - Xml containing description of a device.
 * @param iio_dev - Structure describing a device, channels and attributes.
 * @return SUCCESS in case of success or negative value otherwise.
 */
static ssize_t iio_axi_dac_get_xml(char** xml, struct iio_device *iio_dev)
{
	struct xml_document *document = NULL;
	struct xml_attribute *att;
	struct xml_node *device;
	ssize_t ret;

	ret = xml_create_node(&device, "device");
	if (ret < 0)
		goto error;
	ret = xml_create_attribute(&att, "id", iio_dev->name);
	if (ret < 0)
		goto error;
	ret = xml_add_attribute(device, att);
	if (ret < 0)
		goto error;
	ret = xml_create_attribute(&att, "name", iio_dev->name);
	if (ret < 0)
		goto error;
	ret = xml_add_attribute(device, att);
	if (ret < 0)
		goto error;
	ret = iio_axi_dac_channel_xml(device, iio_dev->num_ch, CH_VOLTGE);
	if (ret < 0)
		goto error;
	ret = iio_axi_dac_channel_xml(device, iio_dev->num_ch * 2, CH_ALTVOLTGE);
	if (ret < 0)
		goto error;
	ret = xml_create_document(&document, device);
	if (ret < 0) {
		if (document)
			xml_delete_document(document);
		goto error;
	}
	*xml = document->buff;

error:
	if (device)
		xml_delete_node(device);

	return ret;
}

/**
 * @brief Delete iio_device.
 * @param iio_device - Structure describing a device, channels and attributes.
 * @return SUCCESS in case of success or negative value otherwise.
 */
static ssize_t iio_axi_dac_delete_device(struct iio_device *iio_device)
{
	uint16_t i = 0;

	if(!iio_device)
		return FAILURE;

	if(iio_device->channels) {
		while (iio_device->channels[i]) {
			if(iio_device->channels[i]->name)
				free(iio_device->channels[i]->name);
			if(iio_device->channels[i])
				free(iio_device->channels[i]);
			i++;
		}
		free(iio_device->channels);
	}
	if(iio_device)
		free(iio_device);

	return SUCCESS;
}

/**
 * @brief Create structure describing a device, channels and attributes.
 * @param device_name - Device name.
 * @param num_ch - Number of channels that the device has.
 * @return iio_device or NULL, in case of failure.
 */
static struct iio_device *iio_axi_dac_create_device(const char *device_name,
		uint16_t num_ch)
{
	struct iio_device *iio_device;
	const uint8_t num_ch_digits = 3;
	char ch_altvoltage[] = "altvoltage";
	char ch_voltage[] = "voltage";
	uint16_t i, len;
	ssize_t ret;

	iio_device = calloc(1, sizeof(struct iio_device));
	if (!iio_device)
		return NULL;

	iio_device->name = device_name;
	iio_device->num_ch = num_ch;
	iio_device->attributes = NULL; /* no device attribute */
	iio_device->channels = calloc(num_ch + num_ch * 2 + 1,
				      sizeof(struct iio_channel *));
	if (!iio_device->channels)
		goto error;

	for (i = 0; i < num_ch; i++) {
		iio_device->channels[i] = calloc(1, sizeof(struct iio_channel));
		if (!iio_device->channels[i])
			goto error;
		len = strlen(ch_voltage) + num_ch_digits + 1;
		iio_device->channels[i]->name = calloc(1, len);
		if (!iio_device->channels[i]->name)
			goto error;
		ret = sprintf(iio_device->channels[i]->name, "%s%d", ch_voltage, i);
		if (ret < 0)
			goto error;
		iio_device->channels[i]->attributes = iio_voltage_attributes;
		iio_device->channels[i]->ch_out = true;
	}

	for (i = num_ch; i < num_ch + num_ch * 2; i++) {
		iio_device->channels[i] = calloc(1, sizeof(struct iio_channel));
		if (!iio_device->channels[i])
			goto error;
		len = strlen(ch_altvoltage) + num_ch_digits + 1;
		iio_device->channels[i]->name = calloc(1, len);
		if (!iio_device->channels[i]->name)
			goto error;
		ret = sprintf(iio_device->channels[i]->name, "%s%d", ch_altvoltage, i - num_ch);
		if (ret < 0)
			goto error;
		iio_device->channels[i]->attributes = iio_altvoltage_attributes;
		iio_device->channels[i]->ch_out = true;
	}

	iio_device->channels[i] = NULL;

	return iio_device;

error:
	iio_axi_dac_delete_device(iio_device);

	return NULL;
}

/**
 * @brief Application for reading/writing and parameterization of
 * axi_dac device.
 * @param desc - Application descriptor.
 * @param param - Application configuration structure.
 * @return SUCCESS in case of success, FAILURE otherwise.
 */
int32_t iio_axi_dac_app_init(struct iio_axi_dac_app_desc **desc,
			     struct iio_axi_dac_app_init_param *param)
{
	struct iio_interface_init_par iio_axi_dac_intf_par;
	struct iio_device *iio_axi_dac_device;
	struct iio_axi_dac *iio_axi_dac_inst;
	int32_t status;

	if (!param)
		return FAILURE;

	if (!param->tx_dac || !param->tx_dmac)
		return FAILURE;

	iio_axi_dac_inst = (struct iio_axi_dac *)calloc(1, sizeof(struct iio_axi_dac));
	if (!iio_axi_dac_inst)
		return FAILURE;

	iio_axi_dac_inst->dac = param->tx_dac;
	iio_axi_dac_inst->dmac = param->tx_dmac;
	iio_axi_dac_inst->dac_ddr_base = param->dac_ddr_base;
	iio_axi_dac_inst->dcache_flush_range = param->dcache_flush_range;

	iio_axi_dac_device = iio_axi_dac_create_device(iio_axi_dac_inst->dac->name,
			     iio_axi_dac_inst->dac->num_channels);
	if (!iio_axi_dac_device)
		goto error_free_iio_axi_dac_inst;

	iio_axi_dac_intf_par = (struct iio_interface_init_par) {
		.dev_name = iio_axi_dac_inst->dac->name,
		.dev_instance = iio_axi_dac_inst,
		.iio_device = iio_axi_dac_device,
		.get_xml = iio_axi_dac_get_xml,
		.transfer_dev_to_mem = NULL,
		.transfer_mem_to_dev = iio_axi_dac_transfer_mem_to_dev,
		.read_data = NULL,
		.write_data = iio_axi_dac_write_dev,
	};

	status = iio_register(&iio_axi_dac_intf_par);
	if(status < 0)
		goto error_free_iio_axi_dac_inst;

	*desc = calloc(1, sizeof(struct iio_axi_dac_app_desc));
	if (!(*desc))
		goto error_free_unregister;

	(*desc)->iio_axi_dac_inst = iio_axi_dac_inst;

	return SUCCESS;

error_free_unregister:
	iio_unregister(iio_axi_dac_inst->dac->name);
error_free_iio_axi_dac_inst:
	free(iio_axi_dac_inst);

	return FAILURE;
}

/**
 * @brief Release resources.
 * @param desc - Application descriptor.
 * @return SUCCESS in case of success, FAILURE otherwise.
 */
int32_t iio_axi_dac_app_remove(struct iio_axi_dac_app_desc *desc)
{
	struct iio_interface *iio_interface;
	struct iio_axi_dac *iio_axi_dac_inst;

	if (!desc)
		return FAILURE;

	iio_interface = iio_unregister(desc->iio_axi_dac_inst->dac->name);
	if (iio_interface) {
		iio_axi_dac_delete_device(iio_interface->iio);
		iio_axi_dac_inst = (struct iio_axi_dac *)iio_interface->dev_instance;
		free(iio_axi_dac_inst);
	}

	free(desc);

	return SUCCESS;
}
