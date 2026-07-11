#ifndef ADXL345_DATA_H_
#define ADXL345_DATA_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/

/********************** macros ***********************************************/
/* Resumen:
* Acelerometro digital de 3 ejes (X, Y, Z)
* Link: https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf
* Funcionamiento simple:
* Configurar el registro de Power CTL en:
* Table 26. Register 0x2D:  
* - Bit 3: Measure = 1  -> ESTE!
* - Bit 2: Self Test = 0
* - Bit 1: Interrupt = 0
* - Bit 0: Link = 0
* Data en: 0x08; // Activa el bit "Measure"
* Una vez activada: data in the DATAX, DATAY, and DATAZ registers (Address 0x32 to Address 0x37).
* leer los datos de los registros de datos (X, Y, Z)
* Table 3. Register 0x32 a 0x37:
* - 16 bits: X data, Y data, Z data  (Data 0 y Data 1 de cada eje)
 */
#define ADXL345_ADDRESS     		(0x53)
#define ADXL345_ADDRESS_SHIFTED     (0x53 << 1)     // Ya lo dejo desplazado

#define ADXL345_REG_DEVID       0x00
#define ADXL345_DEVID_VAL       0xE5

#define ADXL345_REG_POWER_CTL   0x2D
#define ADXL345_REG_POWER_CTL_SET_IN_MEASURE   0x08
#define ADXL345_BASE_REG_DATA      0x32     // Registro de inicio de los datos (X, Y, Z)
#define ADXL345_DATA_LENGTH        6        // 6 bytes: 3 ejes (X, Y, Z) * 2 bytes por ejes (Data 0 y Data 1)
/********************** typedef **********************************************/
typedef struct {
    int16_t  x;
    int16_t  y;
    int16_t  z;
} adxl345_axes_t;
typedef struct {
    uint16_t        i2c_addr;
    bool            initialized;
    bool        	is_valid_sample;
    adxl345_axes_t  sample;
} adxl345_dta_t;


/********************** external data declaration ****************************/
extern adxl345_dta_t adxl345_data;

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* ADXL345_DATA_H_  */

/********************** end of file ******************************************/
