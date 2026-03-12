/*
 * illumos DRM sysfs stub.
 * The modern DRM sysfs layer is Linux-specific (sysfs class, kobjects, uevent).
 * On illumos we provide stubs for the exported API.
 * drm_internal.h provides static inline stubs for the minor/connector add/remove
 * functions under #else (non-__linux__).
 */

/* Public domain. */

#include <linux/types.h>
#include <drm/drm_device.h>
#include <drm/drm_connector.h>
#include <drm/drm_property.h>
#include <drm/drm_sysfs.h>

void
drm_sysfs_hotplug_event(struct drm_device *dev)
{
	(void)dev;
}

void
drm_sysfs_connector_hotplug_event(struct drm_connector *connector)
{
	(void)connector;
}

void
drm_sysfs_connector_status_event(struct drm_connector *connector,
    struct drm_property *property)
{
	(void)connector;
	(void)property;
}

void
drm_sysfs_connector_property_event(struct drm_connector *connector,
    struct drm_property *property)
{
	(void)connector;
	(void)property;
}

void
drm_sysfs_lease_event(struct drm_device *dev)
{
	(void)dev;
}
