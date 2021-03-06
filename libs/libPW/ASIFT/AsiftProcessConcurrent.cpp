/**
*-------------------------------------------------
*  Copyright 2016 by Tidop Research Group <daguilera@usal.se>
*
* This file is part of GRAPHOS - inteGRAted PHOtogrammetric Suite.
*
* GRAPHOS - inteGRAted PHOtogrammetric Suite is free software: you can redistribute
* it and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation, either
* version 3 of the License, or (at your option) any later version.
*
* GRAPHOS - inteGRAted PHOtogrammetric Suite is distributed in the hope that it will
* be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*
* @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
*-------------------------------------------------
*/
#include <QDir>
#include <QFile>
#include "ASIFTParametersDefinitions.h"
#include "AsiftProcessConcurrent.h"
#include "ASIFTkpd.h"
#include "ASIFTkpdConcurrent.h"
#include "ASIFTmactching.h"
#include "AsiftToPastisProcess.h"
#include "MultiProcessConcurrent.h"
#include "Tools/ConvertProcess.h"
#include "ConvertProcess2.h"
#include "ImagePairsFileIO.h"
#include "libPW.h"

using namespace PW;



AsiftProcessConcurrent::AsiftProcessConcurrent(QList<PWImage *> inputImages,                                               
                                               bool resize,
                                               int tilts,
                                               QString pairsFileName,
                                               bool generateReverse,
                                               bool concurrent,
                                               QString outputPath):
    MultiProcess(true),
    mInputImages(inputImages)
{
    setStartupMessage("Searching for tie points (ASIFT)...");
    QString aFilename = inputImages.at(0)->getFileName();
    QString imagesFilePath=inputImages.at(0)->getFullPath();
    QFileInfo fileInfo(imagesFilePath);
    imagesFilePath=fileInfo.absolutePath();
    QString matchesOutputPath = outputPath;
    QDir auxDir=QDir::currentPath();

    if(!auxDir.exists(matchesOutputPath))
    {
        matchesOutputPath=imagesFilePath+"/"+MATCHING_PATH;
        if(!auxDir.exists(matchesOutputPath))
        {
            if(!auxDir.mkdir(matchesOutputPath))
            {
                qCritical() << QObject::tr("Error making directory:\n%1").arg(matchesOutputPath);
                return;
            }
        }
        matchesOutputPath+="/";
        matchesOutputPath+=ASIFTPARAMETERSDEFINITIONS_MATCHES_PATH;
        if(!auxDir.exists(matchesOutputPath))
        {
            if(!auxDir.mkdir(matchesOutputPath))
            {
                qCritical() << QObject::tr("Error making directory:\n%1").arg(matchesOutputPath);
                return;
            }
        }
    }
    QString extension = aFilename.right(aFilename.length()-aFilename.lastIndexOf(".")-1);
    MultiProcess *convertProcess = new MultiProcess(!concurrent);
    MultiProcess *kpdProcess = new MultiProcess(!concurrent);
    MultiProcess *matchingProcess = new MultiProcess(!concurrent);

    QVector<QString> imagesFileToProcess;
    QMap<QString,QVector<QString> > imagePairs;
    if(!pairsFileName.isEmpty())
    {
        ImagePairsFileIO imagePairsFileIO;
        if(IMAGEPAIRSFILEIO_ERROR==imagePairsFileIO.readStandarAsciiFile(pairsFileName))
        {
            qCritical() << QObject::tr("Error reading image pairs file");
            return;
        }
        if(IMAGEPAIRSFILEIO_ERROR==imagePairsFileIO.getImagePairs(imagePairs))
        {
            qCritical() << QObject::tr("Error getting image pairs");
            return;
        }
        QMap<QString,QVector<QString> >::const_iterator iter=imagePairs.constBegin();
        while(iter!=imagePairs.constEnd())
        {
            QString imageFile=imagesFilePath+"/"+iter.key();
            QVector<QString> imagePairsFile=iter.value();
            if(!imagesFileToProcess.contains(imageFile))
                imagesFileToProcess.push_back(imageFile);
            for(int nFile=0;nFile<imagePairsFile.size();nFile++)
            {
                QString auxFile=imagesFilePath+"/"+imagePairsFile[nFile];
                if(!imagesFileToProcess.contains(auxFile))
                    imagesFileToProcess.push_back(auxFile);
            }
            iter++;
        }
        // Compruebo que estan en el proyecto las imagenes
        for(int nFile=0;nFile<imagesFileToProcess.size();nFile++)
        {
            QString imageFileInPair=imagesFileToProcess[nFile];
            QFileInfo auxFileInfo(imageFileInPair);
            QString imageFileInPairBaseName=auxFileInfo.fileName();
            bool findImage=false;
            for(int kk=0;kk<inputImages.count();kk++)
            {
                QString imageFileInProject = inputImages.at(kk)->getFullPath();
                QFileInfo fileInfo(imageFileInProject);
                QString imageFileInProjectBaseName=fileInfo.fileName();
                if(imageFileInProjectBaseName.compare(imageFileInPairBaseName,Qt::CaseInsensitive)==0)
                {
                    findImage=true;
                    break;
                }
            }
            if(!findImage)
            {
                qCritical() << QObject::tr("Not found in project image:\n  %1").arg(imageFileInPair);
                return;
            }
        }
    }
    else
    {
        for(int kk=0;kk<inputImages.count();kk++)
        {
            QString imageFileInProject = inputImages.at(kk)->getFileName();
            imagesFileToProcess.push_back(inputImages.at(kk)->getFullPath());
            if(kk<(inputImages.count()-1))
            {
                QVector<QString> pairs;
                for(int tt=kk+1;tt<inputImages.count();tt++)
                {
                    QString auxImageFileInProject = inputImages.at(tt)->getFileName();
                    pairs.push_back(auxImageFileInProject);
                }
                imagePairs[imageFileInProject]=pairs;
            }
        }
    }
    //Convert to png?
    if(extension.toLower() != "png")
    {
//        for (int i=0; i<inputImages.count(); i++)
        bool applyConvertProcess=false;
        for (int i=0; i<imagesFileToProcess.size(); i++)
        {
//            QString pngFilePath = inputImages.at(i)->getFullPath();
            QString pngFilePath=imagesFileToProcess[i];
            pngFilePath = pngFilePath.left(pngFilePath.lastIndexOf(".")+1)+ "png";
//            convertProcess->appendProcess(new ConvertProcess(inputImages.at(i)->getFullPath(),
            if(!QFile::exists(pngFilePath))
            {
                convertProcess->appendProcess(new ConvertProcess(imagesFileToProcess[i],
                                                                 pngFilePath));
                if(!applyConvertProcess)
                    applyConvertProcess=true;
            }
//            fullPaths.append(pngFilePath);
        }
        if(applyConvertProcess)
            appendProcess(convertProcess);
    }
//    else
//    {
////        for (int i=0; i<inputImages.count(); i++)
//        for (int i=0; i<imagesFileToProcess.size(); i++)
//        {
////            fullPaths.append(inputImages.at(i)->getFullPath());
//            fullPaths.append(imagesFileToProcess.at(i));
//        }
//    }

//    if (outputPath.isEmpty())
//    {
//        QString aFileName = fullPaths.at(0);
//        outputPath = aFileName.left(aFileName.lastIndexOf(QRegExp("/"))+1);
//    }

    // key points detection:
//    for (int i=0; i<inputImages.count(); i++)
    for (int i=0; i<imagesFileToProcess.size(); i++)
    {
        QString pngFilePath=imagesFileToProcess[i];
        pngFilePath = pngFilePath.left(pngFilePath.lastIndexOf(".")+1)+ "png";
//        kpdProcess->appendProcess(new ASIFTkpd(fullPaths.at(i),"",resize,tilts));
        kpdProcess->appendProcess(new ASIFTkpd(pngFilePath,"",resize,tilts));
    }
    appendProcess(kpdProcess);

    // matchings:
    QMap<QString,QVector<QString> >::const_iterator iter=imagePairs.constBegin();
    while(iter!=imagePairs.constEnd())
    {
        QString fileName=iter.key();
        QString pngFilePath=imagesFilePath+"/"+fileName;
        pngFilePath = pngFilePath.left(pngFilePath.lastIndexOf(".")+1)+ "png";
        QDir outputDir(matchesOutputPath+"/"+"Pastis"+fileName);
        if (!outputDir.exists())
        {
            if(!outputDir.mkdir(matchesOutputPath+"/"+"Pastis"+fileName))
            {
                qCritical() << QObject::tr("Error making directory:\n%1").arg(matchesOutputPath+"/"+"Pastis"+fileName);
                return;
            }
        }
        QVector<QString> pairs=iter.value();
        for(int j=0;j<pairs.size();j++)
        {
            QString pairFileName=pairs[j];
            QString pairPngFilePath=imagesFilePath+"/"+pairFileName;
            pairPngFilePath = pairPngFilePath.left(pairPngFilePath.lastIndexOf(".")+1)+ "png";
            matchingProcess->appendProcess(new ASIFTmactching(pngFilePath ,pairPngFilePath,
                                             outputDir.absolutePath() + "/" + pairFileName + "_V.png",
                                             outputDir.absolutePath() + "/" + pairFileName + "_H.png",
                                             outputDir.absolutePath()+ "/" + pairFileName + ".txt"
                                             ));
        }
        iter++;
    }
//    for (int i=0; i < inputImages.count(); i++)
//    {
//        QDir outputDir(outputPath+"/"+"Pastis"+inputImages.at(i)->getFileName());
//        if (!outputDir.exists())
//            outputDir.mkdir(outputPath+"/"+"Pastis"+inputImages.at(i)->getFileName());

//        for (int j = i+1; j< inputImages.count(); j++)
//        {
//            matchingProcess->appendProcess(new ASIFTmactching(fullPaths.at(i) ,fullPaths.at(j),
//                                             outputDir.absolutePath() + "/" + inputImages.at(j)->getFileName() + "_V.png",
//                                             outputDir.absolutePath() + "/" + inputImages.at(j)->getFileName() + "_H.png",
//                                             outputDir.absolutePath()+ "/" + inputImages.at(j)->getFileName() + ".txt"
//                                             ));
//        }
//    }
    appendProcess(matchingProcess);
    iter=imagePairs.constBegin();
    while(iter!=imagePairs.constEnd())
    {
        QString fileName=iter.key();
        QDir outputDir(matchesOutputPath+"/"+"Pastis"+fileName);
        QVector<QString> pairs=iter.value();
        for(int j=0;j<pairs.size();j++)
        {
            QString pairFileName=pairs[j];
            if(generateReverse){
                QDir reverseOutputDir(matchesOutputPath+"/"+"Pastis"+pairFileName);
                if (!reverseOutputDir.exists())
                {
                    if(!reverseOutputDir.mkdir(matchesOutputPath+"/"+"Pastis"+pairFileName))
                    {
                        qCritical() << QObject::tr("Error making directory:\n%1").arg(matchesOutputPath+"/"+"Pastis"+pairFileName);
                        return;
                    }
                }
                appendProcess(new AsiftToPastisProcess(outputDir.absolutePath()+ "/" + pairFileName + ".txt",
                                                       outputDir.absolutePath()+ "/" + pairFileName + ".txt",
                                                       reverseOutputDir.absolutePath() + "/" + fileName + ".txt"));
            }
            else
                appendProcess(new AsiftToPastisProcess(outputDir.absolutePath()+ "/" + pairFileName + ".txt",
                                                       outputDir.absolutePath()+ "/" + pairFileName + ".txt"));
        }
        iter++;
    }

    //    for (int i=0; i < inputImages.count(); i++)
//    {
//        QDir outputDir(outputPath+"/"+"Pastis"+inputImages.at(i)->getFileName());
//        for (int j = i+1; j< inputImages.count(); j++)
//        {
//            // reverse matchings:
//            QDir reverseOutputDir(outputPath+"/"+"Pastis"+inputImages.at(j)->getFileName());
//            if (!reverseOutputDir.exists())
//                reverseOutputDir.mkdir(outputPath+"/"+"Pastis"+inputImages.at(j)->getFileName());
//            appendProcess(new AsiftToPastisProcess(outputDir.absolutePath()+ "/" + inputImages.at(j)->getFileName() + ".txt",
//                                                   outputDir.absolutePath()+ "/" + inputImages.at(j)->getFileName() + ".txt",
//                                                   reverseOutputDir.absolutePath() + "/" + inputImages.at(i)->getFileName() + ".txt"));
//        }
//    }
}
