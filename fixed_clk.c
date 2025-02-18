// SPDX-License-Identifier: GPL-2.0
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>

// Define the XO frequency (100 MHz)
#define XO_FREQ 100000000

static int my_clk_driver_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct clk_hw_onecell_data *clk_data;
    struct clk_hw *xo_clk, *cpu_clk, *uart_clk, *i2c_clk, *gpio_clk, *spi_clk;
    int ret;

    clk_data = devm_kzalloc(dev, struct_size(clk_data, hws, 6), GFP_KERNEL);
    if (!clk_data)
        return -ENOMEM;

    // Register XO clock (Primary clock source)
    xo_clk = clk_hw_register_fixed_rate(dev, "xo_clk", NULL, 0, XO_FREQ);
    if (IS_ERR(xo_clk)) {
        dev_err(dev, "Failed to register XO clock\n");
        return PTR_ERR(xo_clk);
    }
    clk_data->hws[0] = xo_clk;

    // Register CPU clock (A55) using multiplication (12x to 14x range)
    cpu_clk = clk_hw_register_fixed_factor(dev, "cpu_clk", "xo_clk", 0, 12, 1);
    if (IS_ERR(cpu_clk)) {
        dev_err(dev, "Failed to register CPU clock\n");
        ret = PTR_ERR(cpu_clk);
        goto err_unregister_xo;
    }
    clk_data->hws[1] = cpu_clk;

    // Register UART clock (115200 Hz)
    uart_clk = clk_hw_register_fixed_factor(dev, "uart_clk", "xo_clk", 0, 1, 868);
    if (IS_ERR(uart_clk)) {
        dev_err(dev, "Failed to register UART clock\n");
        ret = PTR_ERR(uart_clk);
        goto err_unregister_cpu;
    }
    clk_data->hws[2] = uart_clk;

    // Register I2C clock (100 kHz)
    i2c_clk = clk_hw_register_fixed_factor(dev, "i2c_clk", "xo_clk", 0, 1, 1000);
    if (IS_ERR(i2c_clk)) {
        dev_err(dev, "Failed to register I2C clock\n");
        ret = PTR_ERR(i2c_clk);
        goto err_unregister_uart;
    }
    clk_data->hws[3] = i2c_clk;

    // Register GPIO clock (same as XO)
    gpio_clk = clk_hw_register_fixed_factor(dev, "gpio_clk", "xo_clk", 0, 1, 1);
    if (IS_ERR(gpio_clk)) {
        dev_err(dev, "Failed to register GPIO clock\n");
        ret = PTR_ERR(gpio_clk);
        goto err_unregister_i2c;
    }
    clk_data->hws[4] = gpio_clk;

    // Register SPI clock (10 MHz)
    spi_clk = clk_hw_register_fixed_factor(dev, "spi_clk", "xo_clk", 0, 1, 10);
    if (IS_ERR(spi_clk)) {
        dev_err(dev, "Failed to register SPI clock\n");
        ret = PTR_ERR(spi_clk);
        goto err_unregister_gpio;
    }
    clk_data->hws[5] = spi_clk;

    clk_data->num = 6;

    ret = of_clk_add_hw_provider(dev->of_node, of_clk_hw_onecell_get, clk_data);
    if (ret) {
        dev_err(dev, "Failed to add clock provider\n");
        goto err_unregister_spi;
    }

    dev_info(dev, "Clocks successfully registered and provided\n");
    return 0;

err_unregister_spi:
    clk_hw_unregister_fixed_factor(spi_clk);
err_unregister_gpio:
    clk_hw_unregister_fixed_factor(gpio_clk);
err_unregister_i2c:
    clk_hw_unregister_fixed_factor(i2c_clk);
err_unregister_uart:
    clk_hw_unregister_fixed_factor(uart_clk);
err_unregister_cpu:
    clk_hw_unregister_fixed_factor(cpu_clk);
err_unregister_xo:
    clk_hw_unregister_fixed_rate(xo_clk);
    return ret;
}

static int my_clk_driver_remove(struct platform_device *pdev)
{
    struct clk_hw_onecell_data *clk_data = of_clk_get_hw_onecell_data(pdev->dev.of_node);

    clk_hw_unregister_fixed_factor(clk_data->hws[5]); // SPI clock
    clk_hw_unregister_fixed_factor(clk_data->hws[4]); // GPIO clock
    clk_hw_unregister_fixed_factor(clk_data->hws[3]); // I2C clock
    clk_hw_unregister_fixed_factor(clk_data->hws[2]); // UART clock
    clk_hw_unregister_fixed_factor(clk_data->hws[1]); // CPU clock
    clk_hw_unregister_fixed_rate(clk_data->hws[0]);   // XO clock

    of_clk_del_provider(pdev->dev.of_node);
    dev_info(&pdev->dev, "Clocks unregistered\n");
    return 0;
}

static const struct of_device_id my_clk_of_match[] = {
    { .compatible = "Mirafra,Ramanujan-clk" },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_clk_of_match);

static struct platform_driver my_clk_driver = {
    .driver = {
        .name = "my-clk",
        .of_match_table = my_clk_of_match,
    },
    .probe = my_clk_driver_probe,
    .remove = my_clk_driver_remove,
};

module_platform_driver(my_clk_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mirafra");
MODULE_DESCRIPTION("Clock driver");
/*
Target Frequencies & Corresponding Factors

    CPU (A55) Clock: 1200 MHz – 1400 MHz
        Since XO is 100 MHz, to reach 1200 MHz, we need a multiplier of 12.
        To reach 1400 MHz, we need a multiplier of 14.
        Solution: Use a multiplier and division factor of 1.
            mult = 12 (for 1200 MHz)
            mult = 14 (for 1400 MHz)
            div = 1

    UART Clock: 115200 Hz (115.2 kHz)
        XO = 100 MHz
        We need to divide by: (100,000,000 / 115200) ≈ 868
        Solution: mult = 1, div = 868

    I2C Clock: Standard 100 kHz
        XO = 100 MHz
        We need to divide by: (100,000,000 / 100,000) = 1000
        Solution: mult = 1, div = 1000

    GPIO Clock: Standard (Keep Same as XO)
        GPIO usually operates at the same clock as XO.
        Solution: mult = 1, div = 1

    SPI Clock: Standard (10 MHz or 50 MHz)
        If 10 MHz is needed: div = 10
        If 50 MHz is needed: div = 2
        Solution: mult = 1, div = 10 (or 2 depending on SPI speed)
*/
