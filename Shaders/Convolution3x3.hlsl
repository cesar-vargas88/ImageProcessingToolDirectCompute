Mat Thresholding(Mat Imagen, int v, double k)
{
	Mat	mThresholding = Gris(Imagen.clone());
	uint8_t *ucThresholdingLine = new unsigned char[mThresholding.rows*mThresholding.cols];
	
	Vec3b	vColor;

	//////////////////////////////////////////////////////////////////////////////////////////
	//	Guardamos la imagen en un vector de 1 x el número de pixeles de la imagen original	//
	//////////////////////////////////////////////////////////////////////////////////////////

	for (int x = 0; x < mThresholding.cols; x++)
	{
		for (int y = 0; y < mThresholding.rows; y++)
		{
			// get pixel
			vColor = mThresholding.at<Vec3b>(Point(x, y));

			if ( x % 2 )
			{
				// set pixel
				ucThresholdingLine[(x*mThresholding.cols) + (mThresholding.rows - y)] = vColor[0];
			}
			else
			{
				// set pixel
				ucThresholdingLine[(x*mThresholding.cols) + y] = vColor[0];			
			}
		}
	}
	
	int		nContador = 0;
	double	dPromedio = 0;

	for (int x = 0; x < mThresholding.cols*mThresholding.rows; x++)
	{
		/*for (int y = x; y < x + v; y++)
		{
			if (v % 2)
			{
				if (y - (v / 2) >= 0 && y + (v / 2) + 1 <= mThresholding.cols*mThresholding.rows - 1)
				{
					dPromedio += (double)ucThresholdingLine[y];
					nContador++;
				}
			}
			else
			{
				if (y - (v / 2) >= 0 && y + (v / 2) <= mThresholding.cols*mThresholding.rows - 1)
				{
					dPromedio += (double)ucThresholdingLine[y];
					nContador++;
				}
			}
		}

		dPromedio /= nContador;

		if (dPromedio >= k)
		{
			ucThresholdingLine[x] = 255;
			vColor[0] = 255;
			vColor[1] = 255;
			vColor[2] = 255;
		}
		else
		{
			ucThresholdingLine[x] = 0;
			vColor[0] = 0;
			vColor[1] = 0;
			vColor[2] = 0;
		}*/
		
		//vColor[0] = (uint8_t) round(dPromedio);
		//vColor[1] = (uint8_t) round(dPromedio);
		//vColor[2] = (uint8_t) round(dPromedio);
	   
		vColor[0] = ucThresholdingLine[x];
		vColor[1] = ucThresholdingLine[x];
		vColor[2] = ucThresholdingLine[x];
		
		if ( (x / mThresholding.cols) % 2 )
		{
			// set pixel
			mThresholding.at <Vec3b> ( Point ( x / mThresholding.cols, (mThresholding.cols-1) - ( x % mThresholding.cols ) ) ) = vColor;
		}
		else
		{
			// set pixel		
			mThresholding.at <Vec3b> ( Point ( x / mThresholding.cols,  x % mThresholding.cols ) ) = vColor;
		}
	}

	delete ucThresholdingLine;

	return mThresholding;
}

pkg install control